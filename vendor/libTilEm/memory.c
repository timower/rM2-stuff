/*
 * TilEm II
 *
 * Copyright (c) 2010-2011 Benjamin Moody
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <tilem.h>

/* Memory management */

void
tilem_free(void* p) {
  free(p);
}

void*
tilem_malloc(size_t s) {
  return malloc(s);
}

void*
tilem_realloc(void* p, size_t s) {
  return realloc(p, s);
}

void*
tilem_try_malloc(size_t s) {
  return malloc(s);
}

void*
tilem_malloc0(size_t s) {
  return calloc(s, 1);
}

void*
tilem_try_malloc0(size_t s) {
  return calloc(s, 1);
}

void*
tilem_malloc_atomic(size_t s) {
  return malloc(s);
}

void*
tilem_try_malloc_atomic(size_t s) {
  return malloc(s);
}

/* Logging */

void
tilem_message(TilemCalc* calc, const char* msg, ...) {
  va_list ap;
  va_start(ap, msg);
  fprintf(stderr, "x%c: ", calc->hw.model_id);
  vfprintf(stderr, msg, ap);
  fputc('\n', stderr);
  va_end(ap);
}

void
tilem_warning(TilemCalc* calc, const char* msg, ...) {
  va_list ap;
  va_start(ap, msg);
  fprintf(stderr, "x%c: WARNING: ", calc->hw.model_id);
  vfprintf(stderr, msg, ap);
  fputc('\n', stderr);
  va_end(ap);
}

void
tilem_internal(TilemCalc* calc, const char* msg, ...) {
  va_list ap;
  va_start(ap, msg);
  fprintf(stderr, "x%c: INTERNAL ERROR: ", calc->hw.model_id);
  vfprintf(stderr, msg, ap);
  fputc('\n', stderr);
  va_end(ap);
}
