// Rust port of nixpkgs' setup-etc.pl, for an A/B closure-size comparison
// against the C and Go ports in this repo. Semantics mirror setup-etc.pl.
// Uid/gid lookup and chown go through `uzers`/`nix`, safe wrappers over
// getpwnam(3)/getgrnam(3)/chown(2) - no hand-rolled unsafe FFI.

use std::collections::HashSet;
use std::env;
use std::fs;
use std::io::Write;
use std::os::unix::fs::{symlink, PermissionsExt};
use std::path::{Path, PathBuf};

use nix::unistd::{chown, Gid, Uid};

const STATIC_DIR: &str = "/etc/static";
const STATIC_PREFIX: &str = "/etc/static/";

fn resolve_uid(s: &str) -> u32 {
    if let Some(rest) = s.strip_prefix('+') {
        return rest.parse().unwrap_or(0);
    }
    match uzers::get_user_by_name(s) {
        Some(u) => u.uid(),
        None => {
            eprintln!("warning: unknown user {s}");
            0
        }
    }
}

fn resolve_gid(s: &str) -> u32 {
    if let Some(rest) = s.strip_prefix('+') {
        return rest.parse().unwrap_or(0);
    }
    match uzers::get_group_by_name(s) {
        Some(g) => g.gid(),
        None => {
            eprintln!("warning: unknown group {s}");
            0
        }
    }
}

fn chown_path(path: &Path, uid: u32, gid: u32) -> bool {
    chown(path, Some(Uid::from_raw(uid)), Some(Gid::from_raw(gid))).is_ok()
}

// Atomically create-or-replace `target`, a symlink pointing at `source`.
fn atomic_symlink(source: &Path, target: &Path) -> bool {
    let mut tmp = target.as_os_str().to_os_string();
    tmp.push(".tmp");
    let tmp = PathBuf::from(tmp);
    let _ = fs::remove_file(&tmp);
    if symlink(source, &tmp).is_err() {
        return false;
    }
    if fs::rename(&tmp, target).is_err() {
        let _ = fs::remove_file(&tmp);
        return false;
    }
    true
}

// True if `path` points into /etc/static: either a symlink into
// /etc/static, or a directory whose entries are all (recursively) static.
fn is_static(path: &Path) -> bool {
    let meta = match fs::symlink_metadata(path) {
        Ok(m) => m,
        Err(_) => return false,
    };
    if meta.file_type().is_symlink() {
        return match fs::read_link(path) {
            Ok(target) => target.to_string_lossy().starts_with(STATIC_PREFIX),
            Err(_) => false,
        };
    }
    if meta.is_dir() {
        let entries = match fs::read_dir(path) {
            Ok(e) => e,
            Err(_) => return false,
        };
        for entry in entries.flatten() {
            if !is_static(&entry.path()) {
                return false;
            }
        }
        return true;
    }
    false
}

// Remove dangling symlinks under /etc that point into /etc/static but no
// longer have a corresponding entry in the new /etc/static. Skips
// /etc/nixos, where all the NixOS sources live.
fn cleanup_dangling(dir: &Path) {
    let entries = match fs::read_dir(dir) {
        Ok(e) => e,
        Err(_) => return,
    };
    for entry in entries.flatten() {
        let path = entry.path();
        if path == Path::new("/etc/nixos") {
            continue;
        }
        let meta = match fs::symlink_metadata(&path) {
            Ok(m) => m,
            Err(_) => continue,
        };
        if meta.file_type().is_symlink() {
            if let Ok(target) = fs::read_link(&path) {
                if target.to_string_lossy().starts_with(STATIC_DIR) {
                    let rel = path.strip_prefix("/etc/").unwrap();
                    let x = Path::new(STATIC_DIR).join(rel);
                    let is_link = fs::symlink_metadata(&x)
                        .map(|m| m.file_type().is_symlink())
                        .unwrap_or(false);
                    if !is_link {
                        eprintln!("removing obsolete symlink '{}'...", path.display());
                        let _ = fs::remove_file(&path);
                    }
                }
            }
        } else if meta.is_dir() {
            cleanup_dangling(&path);
        }
    }
}

// Like Perl's read_file(..., chomp => 1, err_mode => 'quiet').
fn read_lines(path: &Path) -> Vec<String> {
    match fs::read_to_string(path) {
        Ok(s) if !s.is_empty() => s
            .trim_end_matches('\n')
            .split('\n')
            .map(|s| s.to_string())
            .collect(),
        _ => Vec::new(),
    }
}

fn read_trimmed(path: &Path) -> Option<String> {
    fs::read_to_string(path)
        .ok()
        .map(|s| s.trim_end_matches(['\n', '\r']).to_string())
}

// Processes a single entry from the $etc tree, creating (or copying) its
// counterpart under /etc. Returns true if `src` is a real directory (so
// the caller should recurse into it).
fn process_entry(
    etc: &Path,
    src: &Path,
    created: &mut HashSet<String>,
    copied: &mut Vec<String>,
    clean_file: &mut fs::File,
    in_nixos_enter: bool,
) -> bool {
    let fn_ = src.strip_prefix(etc).unwrap().to_string_lossy().to_string();

    // nixos-enter sets up /etc/resolv.conf as a bind mount, so skip it.
    if fn_ == "resolv.conf" && in_nixos_enter {
        return false;
    }

    let meta_src = match fs::symlink_metadata(src) {
        Ok(m) => m,
        Err(_) => return false,
    };
    let is_src_symlink = meta_src.file_type().is_symlink();
    let is_src_dir = meta_src.is_dir();

    let target = PathBuf::from(format!("/etc/{fn_}"));
    if let Some(parent) = target.parent() {
        let _ = fs::create_dir_all(parent);
    }
    created.insert(fn_.clone());

    // Rename doesn't work if target is a directory.
    if is_src_symlink {
        if let Ok(tmeta) = fs::symlink_metadata(&target) {
            if tmeta.is_dir() {
                if is_static(&target) {
                    let _ = fs::remove_dir_all(&target);
                } else {
                    eprintln!(
                        "{} directory contains user files. Symlinking may fail.",
                        target.display()
                    );
                }
            }
        }
    }

    let mode_path = PathBuf::from(format!("{}.mode", src.display()));
    if let Some(mode) = read_trimmed(&mode_path) {
        let static_fn = Path::new(STATIC_DIR).join(&fn_);
        if mode == "direct-symlink" {
            match fs::read_link(&static_fn) {
                Ok(link_target) if atomic_symlink(&link_target, &target) => {}
                _ => eprintln!("could not create symlink {}", target.display()),
            }
        } else {
            let uid_str =
                read_trimmed(&PathBuf::from(format!("{}.uid", src.display()))).unwrap_or_default();
            let gid_str =
                read_trimmed(&PathBuf::from(format!("{}.gid", src.display()))).unwrap_or_default();

            let tmp = PathBuf::from(format!("{}.tmp", target.display()));
            let mut ok = fs::copy(&static_fn, &tmp).is_ok();
            let uid = resolve_uid(&uid_str);
            let gid = resolve_gid(&gid_str);
            if ok {
                ok = chown_path(&tmp, uid, gid);
            }
            if ok {
                match u32::from_str_radix(&mode, 8) {
                    Ok(mode_bits) => {
                        ok = fs::set_permissions(&tmp, fs::Permissions::from_mode(mode_bits)).is_ok();
                    }
                    Err(_) => {
                        eprintln!("warning: invalid mode {mode:?} for {}", target.display());
                        ok = false;
                    }
                }
            }
            if ok {
                ok = fs::rename(&tmp, &target).is_ok();
            }
            if !ok {
                eprintln!("could not create target {}", target.display());
                let _ = fs::remove_file(&tmp);
            }
        }
        copied.push(fn_.clone());
        let _ = writeln!(clean_file, "{fn_}");
    } else if is_src_symlink {
        let static_fn = Path::new(STATIC_DIR).join(&fn_);
        if !atomic_symlink(&static_fn, &target) {
            eprintln!("could not create symlink {}", target.display());
        }
    }

    is_src_dir
}

fn walk_etc(
    etc: &Path,
    dir: &Path,
    created: &mut HashSet<String>,
    copied: &mut Vec<String>,
    clean_file: &mut fs::File,
    in_nixos_enter: bool,
) {
    let entries = match fs::read_dir(dir) {
        Ok(e) => e,
        Err(_) => return,
    };
    for entry in entries.flatten() {
        let src = entry.path();
        if process_entry(etc, &src, created, copied, clean_file, in_nixos_enter) {
            walk_etc(etc, &src, created, copied, clean_file, in_nixos_enter);
        }
    }
}

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        eprintln!("usage: {} <etc-path>", args[0]);
        std::process::exit(1);
    }
    let etc = PathBuf::from(&args[1]);

    // Atomically update /etc/static to point at the etc files of the
    // current configuration.
    if !atomic_symlink(&etc, Path::new(STATIC_DIR)) {
        eprintln!("could not create {STATIC_DIR}");
        std::process::exit(1);
    }

    cleanup_dangling(Path::new("/etc"));

    // Use /etc/.clean to keep track of copied files.
    let old_copied = read_lines(Path::new("/etc/.clean"));

    let mut clean_file = match fs::OpenOptions::new()
        .create(true)
        .append(true)
        .open("/etc/.clean")
    {
        Ok(f) => f,
        Err(e) => {
            eprintln!("could not open /etc/.clean: {e}");
            std::process::exit(1);
        }
    };

    let in_nixos_enter = env::var("IN_NIXOS_ENTER").is_ok();

    // For every file in the etc tree, create a corresponding symlink in
    // /etc to /etc/static.
    let mut created = HashSet::new();
    let mut copied = Vec::new();
    walk_etc(&etc, &etc, &mut created, &mut copied, &mut clean_file, in_nixos_enter);

    // Delete files that were copied in a previous version but not in the
    // current one.
    for fn_ in &old_copied {
        if !fn_.is_empty() && !created.contains(fn_) {
            let target = format!("/etc/{fn_}");
            eprintln!("removing obsolete file '{target}'...");
            let _ = fs::remove_file(&target);
        }
    }

    // Rewrite /etc/.clean.
    drop(clean_file);
    copied.sort();
    copied.dedup();
    let mut out = String::new();
    for fn_ in &copied {
        out.push_str(fn_);
        out.push('\n');
    }
    if let Err(e) = fs::write("/etc/.clean", out) {
        eprintln!("warning: could not write /etc/.clean: {e}");
    }

    // Create /etc/NIXOS tag if it doesn't exist.
    let _ = fs::OpenOptions::new()
        .create(true)
        .append(true)
        .open("/etc/NIXOS");
}
