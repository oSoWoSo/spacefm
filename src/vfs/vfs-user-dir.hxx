/*
 *
 * License: See COPYING file
 *
 */

#pragma once

const char* vfs_user_desktop_dir();
const char* vfs_user_template_dir();

const char* vfs_user_home_dir();
const char* vfs_user_cache_dir();
const char* vfs_user_data_dir();
const char* vfs_user_config_dir();
const char* vfs_user_runtime_dir();

const char* const* vfs_system_data_dir();

const char* vfs_current_dir();

std::string vfs_build_path(const std::string& p1);
std::string vfs_build_path(const std::string& p1, const std::string& p2);
std::string vfs_build_path(const std::string& p1, const std::string& p2, const std::string& p3);
std::string vfs_build_path(const std::string& p1, const std::string& p2, const std::string& p3,
                           const std::string& p4);
std::string vfs_build_path(const std::string& p1, const std::string& p2, const std::string& p3,
                           const std::string& p4, const std::string& p5);
std::string vfs_build_path(const std::string& p1, const std::string& p2, const std::string& p3,
                           const std::string& p4, const std::string& p5, const std::string& p6);
