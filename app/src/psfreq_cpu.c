/**
 * @file psfreq_cpu_init.c
 * @author pyamsoft <pyam(dot)soft(at)gmail(dot)com>
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * @section DESCRIPTION
 * Part of the implementation for the psfreq_cpu_type. This module
 * handles the essential setup when a new psfreq_cpu_type is initialized.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "psfreq_cpu.h"
#include "psfreq_log.h"
#include "psfreq_strings.h"
#include "psfreq_util.h"

static char* psfreq_cpu_init_driver(const psfreq_sysfs_type *sysfs);
static unsigned char psfreq_cpu_init_number_cpus(void);
static unsigned char psfreq_cpu_init_has_pstate(const psfreq_cpu_type *cpu);
static char **psfreq_cpu_init_vector(const psfreq_cpu_type *cpu,
                const char *const what);
static char* psfreq_cpu_init_governor(const psfreq_cpu_type *cpu,
                                const psfreq_sysfs_type *sysfs);
static char psfreq_cpu_init_turbo_boost(const psfreq_sysfs_type *sysfs);
static unsigned int psfreq_cpu_init_freq(
                const psfreq_sysfs_type *sysfs,
                const char *const type,
                const char *const what);
static unsigned char psfreq_cpu_init_dynamic(psfreq_cpu_type *cpu,
                const psfreq_sysfs_type *sysfs);

unsigned char psfreq_cpu_init(psfreq_cpu_type *cpu,
                const psfreq_sysfs_type *sysfs)
{
        char *m;
        cpu->scaling_driver = psfreq_cpu_init_driver(sysfs);
        cpu->has_pstate = psfreq_cpu_init_has_pstate(cpu);
        if (cpu->has_pstate == 0) {
                psfreq_log_error("psfreq_cpu_init",
                                "System does not have intel_pstate and is unsupported");
                free(cpu->scaling_driver);
                return 0;
        }
        cpu->cpu_num = psfreq_cpu_init_number_cpus();
        cpu->vector_scaling_min_freq = psfreq_cpu_init_vector(cpu, "min_freq");
        cpu->vector_scaling_max_freq = psfreq_cpu_init_vector(cpu, "max_freq");
        cpu->vector_scaling_governor = psfreq_cpu_init_vector(cpu, "governor");
        cpu->cpuinfo_max_freq = psfreq_cpu_init_freq(sysfs, "cpuinfo",
                                                        "max");
        cpu->cpuinfo_min_freq = psfreq_cpu_init_freq(sysfs, "cpuinfo",
                                                        "min");
        m = psfreq_sysfs_read(sysfs, "intel_pstate/max_perf_pct");
        cpu->pst_max = psfreq_strings_to_int(m);
        free(m);

        m = psfreq_sysfs_read(sysfs, "intel_pstate/min_perf_pct");
        cpu->pst_min = psfreq_strings_to_int(m);
        free(m);

        return psfreq_cpu_init_dynamic(cpu, sysfs);
}

static unsigned char psfreq_cpu_init_dynamic(psfreq_cpu_type *cpu,
                const psfreq_sysfs_type *sysfs)
{
        cpu->scaling_max_freq = psfreq_cpu_init_freq(sysfs, "scaling",
                                                        "max");
        cpu->scaling_min_freq = psfreq_cpu_init_freq(sysfs, "scaling",
                                                        "min");
        cpu->scaling_governor = psfreq_cpu_init_governor(cpu, sysfs);
        cpu->pst_turbo = psfreq_cpu_init_turbo_boost(sysfs);
        return 1;
}

unsigned char psfreq_cpu_reinit(psfreq_cpu_type *cpu,
                const psfreq_sysfs_type *sysfs)
{
        /*
         * Need to free scaling governor before re-mallocing
         */
        free(cpu->scaling_governor);
        return psfreq_cpu_init_dynamic(cpu, sysfs);
}

void psfreq_cpu_destroy(psfreq_cpu_type *cpu)
{
        unsigned char i;
        psfreq_log_debug("psfreq_cpu_destroy",
                        "Free all allocated memory");
        for (i = 0; i < cpu->cpu_num; ++i) {
                psfreq_log_debug("psfreq_cpu_destroy",
                                "free vector_scaling_min_freq[%u]", i);
                free(cpu->vector_scaling_min_freq[i]);

                psfreq_log_debug("psfreq_cpu_destroy",
                                "free vector_scaling_max_freq[%u]", i);
                free(cpu->vector_scaling_max_freq[i]);

                psfreq_log_debug("psfreq_cpu_destroy",
                                "free vector_scaling_governor[%u]", i);
                free(cpu->vector_scaling_governor[i]);
        }

        psfreq_log_debug("psfreq_cpu_destroy",
                        "free vector_scaling_min_freq");
        free(cpu->vector_scaling_min_freq);

        psfreq_log_debug("psfreq_cpu_destroy",
                        "free vector_scaling_max_freq");
        free(cpu->vector_scaling_max_freq);

        psfreq_log_debug("psfreq_cpu_destroy",
                        "free vector_scaling_governor");
        free(cpu->vector_scaling_governor);

        psfreq_log_debug("psfreq_cpu_destroy",
                        "free scaling_governor");
        free(cpu->scaling_governor);

        psfreq_log_debug("psfreq_cpu_destroy",
                        "free scaling_driver");
        free(cpu->scaling_driver);
}

/*
 * Find the total number of CPUS (logical and physical) that exist on the
 * system.
 */
static unsigned char psfreq_cpu_init_number_cpus(void)
{
        const char *const cmd = "grep processor /proc/cpuinfo | wc -l";
        const unsigned char size = 1;
        char **res = psfreq_util_read_pipe(cmd, &size);
        unsigned int n;

        if (res == NULL) {
                psfreq_log_error("psfreq_cpu_init_number_cpus",
                                "Failed to find number of cpus");
                return 0;
        }

        n = psfreq_strings_to_uint(res[0]);
        psfreq_log_debug("psfreq_cpu_init_number_cpus",
                        "Free memory held by res");
        free(res[0]);
        free(res);
        psfreq_log_debug("psfreq_cpu_init_number_cpus",
                        "Number of cpus: %u", n);
        return n;
}

static char* psfreq_cpu_init_driver(const psfreq_sysfs_type *sysfs)
{
        char *driver;

        if (sysfs == NULL) {
                psfreq_log_error("psfreq_cpu_init_has_pstate",
                                "sysfs is NULL");
                return 0;
        }
        driver = psfreq_sysfs_read(sysfs, "cpu0/cpufreq/scaling_driver");
        if (driver == NULL) {
                psfreq_log_error("psfreq_cpu_init_has_pstate",
                                "Unable to check for intel_pstate driver");
                return 0;
        }

        return driver;
}

static unsigned char psfreq_cpu_init_has_pstate(const psfreq_cpu_type *cpu)
{

        const char *const cmp = "intel_pstate";
        unsigned char r;
        const char *driver = cpu->scaling_driver;
        psfreq_log_debug("psfreq_cpu_init_has_pstate",
                        "Compare driver '%s' with '%s'", driver, cmp);
        r = (strcmp(driver, cmp) == 0);
        return r;
}

static unsigned int psfreq_cpu_init_freq(
                const psfreq_sysfs_type *sysfs,
                const char *const type,
                const char *const what)
{
        char *line;
        char *f;
        unsigned int result;
        if (sysfs == NULL) {
                psfreq_log_error("psfreq_cpu_init_freq",
                                "sysfs is NULL");
                return 0;
        }

        if (psfreq_strings_asprintf(&f, "cpu0/cpufreq/%s_%s_freq", type, what) < 0) {
                psfreq_log_error("psfreq_cpu_init_freq",
                        "asprintf returned a -1, indicating a failure during\n"
                        "either memory allocation or some other error.");
                return 0;
        }

        line = psfreq_sysfs_read(sysfs, f);
        free(f);
        if (line == NULL) {
                psfreq_log_error("psfreq_cpu_init_freq",
                                "Unable to read for %s_%s_freq", type, what);
                return 0;
        }
        result = psfreq_strings_to_uint(line);
        if (result == 0) {
                psfreq_log_error("psfreq_cpu_init_freq",
                                "Unable to convert string '%s' to uint",
                                line);
        }

        free(line);
        return result;
}

static char **psfreq_cpu_init_vector(const psfreq_cpu_type *cpu,
                const char *const what)
{
        unsigned char num;
        unsigned char i;
        char **vector;

        psfreq_log_debug("psfreq_cpu_init_vector", "Check for non-NULL cpu");
        if (cpu == NULL) {
                psfreq_log_error("psfreq_cpu_init_vector",
                                "cpu is NULL, failed to find cpu number");
                return NULL;
        }
        psfreq_log_debug("psfreq_cpu_init_vector", "Check for non-zero size");
        num = cpu->cpu_num;
        if (num == 0) {
                psfreq_log_error("psfreq_cpu_init_vector",
                                "Size is 0, failed to find cpu number");
                return NULL;
        }

        psfreq_log_debug("psfreq_cpu_init_vector",
                        "malloc for vector");
        vector = malloc(num * sizeof(char *));
        if (vector == NULL) {
                psfreq_log_error("psfreq_cpu_init_vector",
                                "Failed to malloc for vector");
                return NULL;
        }

        for (i = 0; i < num; ++i) {
                char *buf = NULL;
                if (psfreq_strings_asprintf(&buf, "cpu%u/cpufreq/scaling_%s", i, what) < 0) {
                        psfreq_log_error("psfreq_cpu_init_vector",
                                "asprintf returned a -1, indicating a failure "
                                "during\n either memory allocation or some "
                                "other error.");
                        free(vector);
                        return NULL;
                }

                psfreq_log_debug("psfreq_cpu_init_vector",
                                "assign '%s' to vector index %d", buf, i);
                vector[i] = buf;
        }
        return vector;
}

unsigned int psfreq_cpu_get_cpuinfo_min(const psfreq_cpu_type *cpu)
{
        unsigned int min;
        unsigned int max;
        if (cpu == NULL) {
                psfreq_log_error("psfreq_cpu_get_cpuinfo_min",
                                "cpu is NULL");
                return 0;
        }
        min = cpu->cpuinfo_min_freq;
        max = cpu->cpuinfo_max_freq;
        return ((double) min / max) * 100;
}

unsigned int psfreq_cpu_get_cpuinfo_max(void)
{
        /* Hardcoded to 100 */
        return 100;
}

unsigned int psfreq_cpu_get_scaling_min(const psfreq_cpu_type *cpu)
{
        unsigned int min;
        unsigned int max;
        if (cpu == NULL) {
                psfreq_log_error("psfreq_cpu_get_scaling_min",
                                "cpu is NULL");
                return 0;
        }
        min = cpu->scaling_min_freq;
        max = cpu->cpuinfo_max_freq;
        return ((double) min / max) * 100;
}

unsigned int psfreq_cpu_get_scaling_max(const psfreq_cpu_type *cpu)
{
        unsigned int min;
        unsigned int max;
        if (cpu == NULL) {
                psfreq_log_error("psfreq_cpu_get_scaling_max",
                                "cpu is NULL");
                return 0;
        }
        min = cpu->scaling_max_freq;
        max = cpu->cpuinfo_max_freq;
        return ((double) min / max) * 100;
}

static char* psfreq_cpu_init_governor(const psfreq_cpu_type *cpu,
                                const psfreq_sysfs_type *sysfs)
{
        const char* f;
        if (cpu == NULL) {
                psfreq_log_error("psfreq_cpu_init_governor",
                                "cpu is NULL");
                return NULL;
        }
        if (cpu->vector_scaling_governor == NULL) {
                psfreq_log_error("psfreq_cpu_init_governor",
                                "cpu->vector_scaling_governor is NULL");
                return NULL;
        }

        if (sysfs == NULL) {
                psfreq_log_error("psfreq_cpu_init_governor",
                                "sysfs is NULL");
                return NULL;
        }
        f = cpu->vector_scaling_governor[0];
        return psfreq_sysfs_read(sysfs, f);
}

static char psfreq_cpu_init_turbo_boost(const psfreq_sysfs_type *sysfs)
{
        char turbo;
        char *line;
        if (sysfs == NULL) {
                psfreq_log_error("psfreq_cpu_init_turbo_boost",
                                "sysfs is NULL");
                return -1;
        }
        line = psfreq_sysfs_read(sysfs, "intel_pstate/no_turbo");
        if (line == NULL) {
                psfreq_log_error("psfreq_cpu_init_turbo_boost",
                                "Could not discover turbo_boost value");
                return -1;
        }

        turbo = psfreq_strings_to_int(line);
        free(line);
        return turbo;
}

unsigned char psfreq_cpu_set_max(const psfreq_cpu_type *cpu,
                                 const psfreq_sysfs_type *sysfs,
                                 const int *const m)
{
        unsigned char i;
        int freq;
        int n;
        if (*m >= 100) {
                n = psfreq_cpu_get_cpuinfo_max();
        } else if (*m <= 0) {
                n = psfreq_cpu_get_cpuinfo_min(cpu) + 1;
        } else {
                n = *m;
        }
        freq = cpu->cpuinfo_max_freq * ((double) n / 100);
        if (sysfs == NULL) {
                psfreq_log_error("psfreq_cpu_set_max",
                                "sysfs is NULL");
                return 0;
        }
        if (cpu == NULL) {
                psfreq_log_error("psfreq_cpu_set_max",
                                "cpu is NULL");
                return 0;
        }
        if (cpu->cpu_num == 0) {
                psfreq_log_error("psfreq_cpu_set_max",
                                "cpu->cpu_num is 0");
                return 0;
        }
        if (cpu->vector_scaling_max_freq == NULL) {
                psfreq_log_error("psfreq_cpu_set_max",
                                "cpu->vector_scaling_max_freq is NULL");
                return 0;
        }
        if (!psfreq_sysfs_write_num(sysfs, "intel_pstate/max_perf_pct", &n)) {
                psfreq_log_error("psfreq_cpu_set_max",
                                "write failed");
                return 0;
        }
        for (i = 0; i < cpu->cpu_num; ++i) {
                if (!psfreq_sysfs_write_num(sysfs,
                                        cpu->vector_scaling_max_freq[i], &freq)) {
                        psfreq_log_error("psfreq_cpu_set_max",
                                        "cpu->vector_scaling_max_freq[i] is NULL");
                        return 0;
                }
        }
        return 1;
}

unsigned char psfreq_cpu_set_min(const psfreq_cpu_type *cpu,
                                 const psfreq_sysfs_type *sysfs,
                                 const int *const m)
{
        unsigned char i;
        int freq;
        int n;
        if (*m >= 100) {
                n = psfreq_cpu_get_cpuinfo_max() - 1;
        } else if (*m <= 0) {
                n = psfreq_cpu_get_cpuinfo_min(cpu);
        } else {
                n = *m;
        }
        freq = cpu->cpuinfo_max_freq * ((double) n / 100);
        if (sysfs == NULL) {
                psfreq_log_error("psfreq_cpu_set_min",
                                "sysfs is NULL");
                return 0;
        }
        if (cpu == NULL) {
                psfreq_log_error("psfreq_cpu_set_min",
                                "cpu is NULL");
                return 0;
        }
        if (cpu->cpu_num == 0) {
                psfreq_log_error("psfreq_cpu_set_min",
                                "cpu->cpu_num is 0");
                return 0;
        }
        if (cpu->vector_scaling_min_freq == NULL) {
                psfreq_log_error("psfreq_cpu_set_min",
                                "cpu->vector_scaling_min_freq is NULL");
                return 0;
        }
        if (!psfreq_sysfs_write_num(sysfs, "intel_pstate/min_perf_pct", &n)) {
                psfreq_log_error("psfreq_cpu_set_min",
                                "write failed");
                return 0;
        }
        for (i = 0; i < cpu->cpu_num; ++i) {
                if (!psfreq_sysfs_write_num(sysfs,
                                        cpu->vector_scaling_min_freq[i], &freq)) {
                        psfreq_log_error("psfreq_cpu_set_min",
                                        "cpu->vector_scaling_min_freq[i] is NULL");
                        return 0;
                }
        }
        return 1;
}

unsigned char psfreq_cpu_set_gov(const psfreq_cpu_type *cpu,
                                 const psfreq_sysfs_type *sysfs,
                                 const char *const m)
{
        unsigned char i;
        if (sysfs == NULL) {
                psfreq_log_error("psfreq_cpu_set_gov",
                                "sysfs is NULL");
                return 0;
        }
        if (cpu == NULL) {
                psfreq_log_error("psfreq_cpu_set_gov",
                                "cpu is NULL");
                return 0;
        }
        if (cpu->cpu_num == 0) {
                psfreq_log_error("psfreq_cpu_set_gov",
                                "cpu->cpu_num is 0");
                return 0;
        }
        if (cpu->vector_scaling_governor == NULL) {
                psfreq_log_error("psfreq_cpu_set_gov",
                                "cpu->vector_scaling_governor is NULL");
                return 0;
        }
        for (i = 0; i < cpu->cpu_num; ++i) {
                if (!psfreq_sysfs_write(sysfs,
                                        cpu->vector_scaling_governor[i], m)) {
                        psfreq_log_error("psfreq_cpu_set_gov",
                                        "cpu->vector_scaling_governor[i] is NULL");
                        return 0;
                }
        }
        return 1;

}

unsigned char psfreq_cpu_set_turbo(const psfreq_sysfs_type *sysfs,
                                   const int *const m)
{
        if (sysfs == NULL) {
                psfreq_log_error("psfreq_cpu_set_turbo",
                                "sysfs is NULL");
                return 0;
        }
        if (!psfreq_sysfs_write_num(sysfs, "intel_pstate/no_turbo", m)) {
                return 0;
        }
        return 1;
}

char **psfreq_cpu_get_real_freqs(const psfreq_cpu_type *cpu)
{
        const char *cmd = "grep MHz /proc/cpuinfo | cut -c12-";
        const unsigned char size = cpu->cpu_num;
        char **res;
        if (size == 0) {
                psfreq_log_error("psfreq_cpu_get_real_freqs",
                                "Failed to find number of cpus");
                return NULL;
        }
        res = psfreq_util_read_pipe(cmd, &size);

        if (res == NULL) {
                psfreq_log_error("psfreq_cpu_get_real_freqs",
                                "Failed to get realtime frequencies");
                return NULL;
        }
        return res;
}

