// Rust rewrite of nix/pkgs/xochitlEnv.nix's bash script, run as a setuid
// security wrapper instead of via sudo (see nix/modules/xochitl-env.nix).
// Builds the chroot xochitl/rm-sync/rm2fb_server run in, then execve()s the
// requested program inside it - optionally dropped to the real invoking
// user, if any.

use std::env;
use std::ffi::OsString;
use std::fmt::Display;
use std::fs;
use std::os::unix::process::CommandExt;
use std::path::{Path, PathBuf};
use std::process::Command;

use nix::mount::{mount, MsFlags};
use nix::sched::{unshare, CloneFlags};
use nix::sys::resource::{rlim_t, setrlimit, Resource};
use nix::unistd::{chroot, getgroups, getuid, setgid, setgroups, setuid, User};

// Baked in by nix/pkgs/xochitlEnv.nix at build time - absolute store path,
// since $PATH can't be trusted in a setuid context.
const BLKID_PATH: &str = env!("BLKID_PATH");

const PATH: &str = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin";
const HOME: &str = "/home/root";

const ROOT: &str = "/run/xochitlEnv";
const ACTIVE_PARTITION_FILE: &str = "/run/active-partition";
const DEFAULT_PARTITION: &str = "/dev/mmcblk2p2";
const UBOOT_PARTITION: &str = "/dev/mmcblk2p1";
const HOME_PARTITION: &str = "/dev/mmcblk2p4";

#[derive(Debug, PartialEq, Eq)]
enum EnvSpec {
    Literal(String, String),
    // Passed through from the wrapper's own environment if set, else omitted.
    Passthrough(String),
}

fn parse_env_spec(spec: &str) -> EnvSpec {
    match spec.split_once('=') {
        Some((name, value)) => EnvSpec::Literal(name.to_string(), value.to_string()),
        None => EnvSpec::Passthrough(spec.to_string()),
    }
}

// Parses `[--env SPEC | --rtprio VALUE]... -- PROGRAM [ARGS...]`.
fn parse_args(args: &[String]) -> Result<(Vec<EnvSpec>, Option<rlim_t>, String, Vec<String>), String> {
    let mut specs = Vec::new();
    let mut rtprio = None;
    let mut i = 0;
    while i < args.len() {
        match args[i].as_str() {
            "--env" => {
                let value = args.get(i + 1).ok_or("--env requires an argument")?;
                specs.push(parse_env_spec(value));
                i += 2;
            }
            "--rtprio" => {
                let value = args.get(i + 1).ok_or("--rtprio requires an argument")?;
                rtprio = Some(
                    value
                        .parse::<rlim_t>()
                        .map_err(|e| format!("invalid --rtprio value {value:?}: {e}"))?,
                );
                i += 2;
            }
            "--" => {
                let (program, rest) =
                    args[i + 1..].split_first().ok_or("missing PROGRAM after --")?;
                return Ok((specs, rtprio, program.clone(), rest.to_vec()));
            }
            other => return Err(format!("unexpected argument: {other}")),
        }
    }
    Err("missing `-- PROGRAM`".to_string())
}

fn die(what: &str, err: impl Display) -> ! {
    eprintln!("xochitl-env: {what}: {err}");
    std::process::exit(1);
}

fn must<T, E: Display>(what: &str, result: Result<T, E>) -> T {
    result.unwrap_or_else(|e| die(what, e))
}

fn mount_or_die(
    what: &str,
    source: Option<&str>,
    target: &Path,
    fstype: Option<&str>,
    flags: MsFlags,
) {
    must(what, mount(source, target, fstype, flags, None::<&str>));
}

fn main() {
    let args: Vec<String> = env::args().collect();
    let (env_specs, rtprio, program, program_args) = match parse_args(&args[1..]) {
        Ok(v) => v,
        Err(e) => {
            eprintln!("xochitl-env: {e}");
            eprintln!(
                "usage: xochitl-env [--env NAME | --env NAME=VALUE | --rtprio LIMIT]... -- PROGRAM [ARGS...]"
            );
            std::process::exit(1);
        }
    };

    // Private mount namespace, so mounts don't leak and are cleaned up on exit.
    must("unshare", unshare(CloneFlags::CLONE_NEWNS));
    mount_or_die(
        "make / private",
        None,
        Path::new("/"),
        None,
        MsFlags::MS_REC | MsFlags::MS_PRIVATE,
    );

    let root = PathBuf::from(ROOT);
    must("mkdir root", fs::create_dir_all(&root));

    let active_partition = fs::read_to_string(ACTIVE_PARTITION_FILE)
        .map(|s| s.trim().to_string())
        .unwrap_or_else(|_| DEFAULT_PARTITION.to_string());

    // Mount partitions, read only to prevent changes.
    mount_or_die(
        "mount active partition",
        Some(active_partition.as_str()),
        &root,
        Some("ext4"),
        MsFlags::empty(),
    );
    // On first boot, /nix might not exist.
    must("mkdir root/nix", fs::create_dir_all(root.join("nix")));
    mount_or_die(
        "remount root read-only",
        None,
        &root,
        None,
        MsFlags::MS_REMOUNT | MsFlags::MS_RDONLY | MsFlags::MS_BIND,
    );

    mount_or_die(
        "mount uboot partition",
        Some(UBOOT_PARTITION),
        &root.join("var/lib/uboot"),
        Some("vfat"),
        MsFlags::MS_RDONLY,
    );

    // Mount /home rw, to allow document access.
    mount_or_die(
        "mount home partition",
        Some(HOME_PARTITION),
        &root.join("home"),
        Some("ext4"),
        MsFlags::empty(),
    );

    // Real uid survives a setuid exec untouched, so this identifies the
    // invoking user (root when launched directly, e.g. by systemd).
    let real_uid = getuid();
    let target_user = if real_uid.is_root() {
        None
    } else {
        let user = must("look up invoking user", User::from_uid(real_uid));
        let user = user.unwrap_or_else(|| {
            die(
                "look up invoking user",
                format!("no passwd entry for uid {}", real_uid.as_raw()),
            )
        });
        let groups = must("getgroups", getgroups());
        Some((user, groups))
    };

    if let Some((user, _)) = &target_user {
        if user.dir.is_dir() {
            // Run blkid on the home partition as root, otherwise xochitl fails.
            let status = must(
                "run blkid",
                Command::new(BLKID_PATH).arg(HOME_PARTITION).status(),
            );
            if !status.success() {
                die("run blkid", format!("exited with {status}"));
            }
            mount_or_die(
                "remount root/home read-only",
                None,
                &root.join("home"),
                None,
                MsFlags::MS_REMOUNT | MsFlags::MS_RDONLY | MsFlags::MS_BIND,
            );
            mount_or_die(
                "bind invoking user's home",
                Some(user.dir.to_str().expect("home dir is not valid UTF-8")),
                &root.join("home/root"),
                None,
                MsFlags::MS_BIND,
            );
        }
    }

    // Mount special fs, recursively so nested mounts (e.g. a virtio-9p
    // /nix/store share) propagate too.
    for (source, target) in [
        ("/dev", "dev"),
        ("/sys", "sys"),
        ("/proc", "proc"),
        ("/nix", "nix"),
    ] {
        mount_or_die(
            "rbind mount",
            Some(source),
            &root.join(target),
            None,
            MsFlags::MS_BIND | MsFlags::MS_REC,
        );
    }
    mount_or_die(
        "remount root/nix read-only",
        None,
        &root.join("nix"),
        None,
        MsFlags::MS_REMOUNT | MsFlags::MS_RDONLY | MsFlags::MS_BIND,
    );

    // /tmp needed for xochitl, and the rm-sync service
    // rm.synchronizer: Synchronizer's libraryLock path="/tmp/library-enumeration.lock"
    mount_or_die(
        "bind /tmp",
        Some("/tmp"),
        &root.join("tmp"),
        None,
        MsFlags::MS_BIND,
    );
    // /run for rm2fb.sock and dbus access.
    mount_or_die(
        "bind /run",
        Some("/run"),
        &root.join("run"),
        None,
        MsFlags::MS_BIND,
    );

    must("chroot", chroot(&root));
    must("chdir", env::set_current_dir("/"));

    if let Some(limit) = rtprio {
        // Raising the hard limit needs CAP_SYS_RESOURCE (i.e. still root
        // here) - it survives the setuid() drop below unchanged, letting
        // the exec'd program's own SCHED_FIFO calls succeed without it.
        must("setrlimit rtprio", setrlimit(Resource::RLIMIT_RTPRIO, limit, limit));
    }

    if let Some((user, groups)) = &target_user {
        must("setgroups", setgroups(groups));
        must("setgid", setgid(user.gid));
        must("setuid", setuid(user.uid));
    }

    let mut command = Command::new(&program);
    command.args(&program_args);
    command.env_clear();
    command.env("PATH", PATH);
    command.env("HOME", HOME);
    for spec in &env_specs {
        match spec {
            EnvSpec::Literal(name, value) => {
                command.env(name, value);
            }
            EnvSpec::Passthrough(name) => {
                if let Some(value) = env::var_os(name) {
                    command.env(name, value);
                }
            }
        }
    }

    let err = command.exec();
    let program_display: OsString = program.into();
    die(
        &format!("exec {}", program_display.to_string_lossy()),
        err,
    );
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn env_spec_literal() {
        assert_eq!(
            parse_env_spec("FOO=bar"),
            EnvSpec::Literal("FOO".to_string(), "bar".to_string())
        );
    }

    #[test]
    fn env_spec_literal_with_equals_in_value() {
        assert_eq!(
            parse_env_spec("FOO=bar=baz"),
            EnvSpec::Literal("FOO".to_string(), "bar=baz".to_string())
        );
    }

    #[test]
    fn env_spec_passthrough() {
        assert_eq!(
            parse_env_spec("NOTIFY_SOCKET"),
            EnvSpec::Passthrough("NOTIFY_SOCKET".to_string())
        );
    }

    fn strs(v: &[&str]) -> Vec<String> {
        v.iter().map(|s| s.to_string()).collect()
    }

    #[test]
    fn parses_program_with_no_env() {
        let (specs, rtprio, program, args) =
            parse_args(&strs(&["--", "/usr/bin/xochitl"])).unwrap();
        assert!(specs.is_empty());
        assert_eq!(rtprio, None);
        assert_eq!(program, "/usr/bin/xochitl");
        assert!(args.is_empty());
    }

    #[test]
    fn parses_env_and_program_args() {
        let (specs, rtprio, program, args) = parse_args(&strs(&[
            "--env",
            "LD_PRELOAD=/foo.so",
            "--env",
            "NOTIFY_SOCKET",
            "--",
            "/usr/bin/rm2fb_server",
            "--flag",
        ]))
        .unwrap();
        assert_eq!(
            specs,
            vec![
                EnvSpec::Literal("LD_PRELOAD".to_string(), "/foo.so".to_string()),
                EnvSpec::Passthrough("NOTIFY_SOCKET".to_string()),
            ]
        );
        assert_eq!(rtprio, None);
        assert_eq!(program, "/usr/bin/rm2fb_server");
        assert_eq!(args, vec!["--flag".to_string()]);
    }

    #[test]
    fn parses_rtprio() {
        let (specs, rtprio, program, args) =
            parse_args(&strs(&["--rtprio", "99", "--", "/usr/bin/xochitl"])).unwrap();
        assert!(specs.is_empty());
        assert_eq!(rtprio, Some(99));
        assert_eq!(program, "/usr/bin/xochitl");
        assert!(args.is_empty());
    }

    #[test]
    fn invalid_rtprio_is_an_error() {
        assert!(parse_args(&strs(&["--rtprio", "not-a-number", "--", "/usr/bin/xochitl"])).is_err());
    }

    #[test]
    fn missing_separator_is_an_error() {
        assert!(parse_args(&strs(&["--env", "FOO=bar"])).is_err());
    }

    #[test]
    fn missing_program_after_separator_is_an_error() {
        assert!(parse_args(&strs(&["--"])).is_err());
    }
}
