/*
 * This file is part of the WarheadCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "DllLoadMgr.h"
#include "Errors.h"
#include "FileUtil.h"
#include "Log.h"
#include "StopWatch.h"
#include <filesystem>
#include <optional>
#include <memory>

#if WARHEAD_PLATFORM == WARHEAD_PLATFORM_WINDOWS
#include <windows.h>
#else // Posix and Apple
#include <dlfcn.h>
#endif

namespace fs = std::filesystem;

// Returns "" on Windows and "lib" on posix.
static char const* GetSharedLibraryPrefix()
{
#if WARHEAD_PLATFORM == WARHEAD_PLATFORM_WINDOWS
    return "";
#else // Posix
    return "lib";
#endif
}

// Returns "dll" on Windows, "dylib" on OS X, and "so" on posix.
static char const* GetSharedLibraryExtension()
{
#if WARHEAD_PLATFORM == WARHEAD_PLATFORM_WINDOWS
    return "dll";
#elif WARHEAD_PLATFORM == WARHEAD_PLATFORM_APPLE
    return "dylib";
#else // Posix
    return "so";
#endif
}

#if WARHEAD_PLATFORM == WARHEAD_PLATFORM_WINDOWS
typedef HMODULE HandleType;
#else // Posix
typedef void* HandleType;
#endif

class SharedLibraryUnloader
{
public:
    explicit SharedLibraryUnloader(fs::path path) :
        path_(std::move(path)) { }

    SharedLibraryUnloader(fs::path path, std::optional<fs::path> cache_path) :
        path_(std::move(path)), cache_path_(std::move(cache_path)) { }

    void operator()(HandleType handle) const
    {
        // Unload the associated shared library.
#if WARHEAD_PLATFORM == WARHEAD_PLATFORM_WINDOWS
        bool success = (FreeLibrary(handle) != 0);
#else // Posix
        bool success = (dlclose(handle) == 0);
#endif

        if (!success)
        {
            LOG_ERROR("scripts.hotswap", "Failed to unload (syscall) the shared library \"{}\".", path_.generic_string());
            return;
        }

        /// When the shared library was cached delete it's shared version
        if (cache_path_)
        {
            std::error_code error;
            if (!fs::remove(*cache_path_, error))
            {
                LOG_ERROR("dll", "Failed to delete the cached shared library \"{}\" ({})", cache_path_->generic_string(), error.message());
                return;
            }

            LOG_DEBUG("scripts.hotswap", "Lazy unloaded the shared library \"{}\" and deleted it's cached version at \"{}\"",
                path_.generic_string(), cache_path_->generic_string());
        }
        else
        {
            LOG_TRACE("scripts.hotswap", "Lazy unloaded the shared library \"{}\".",
                      path_.generic_string());
        }
    }

private:
    fs::path const path_;
    std::optional<fs::path> const cache_path_;
};

using HandleHolder = std::unique_ptr<typename std::remove_pointer<HandleType>::type, SharedLibraryUnloader>;

typedef char const* (*GetScriptRevisionHashType)();
typedef void (*AddScriptsType)();
typedef char const* (*GetScriptNameType)();

class ScriptModule
{
public:
    explicit ScriptModule(HandleHolder handle, GetScriptRevisionHashType getScriptRevisionHash,
        AddScriptsType addScripts, GetScriptNameType getScriptNameType, fs::path const& path) :
        _handle(std::forward<HandleHolder>(handle)), _getScriptModuleRevisionHash(getScriptRevisionHash),
        _addScripts(addScripts), _getScriptName(getScriptNameType), _path(path) { }

    ScriptModule(ScriptModule const&) = delete;
    ScriptModule(ScriptModule&& right) = delete;

    ScriptModule& operator=(ScriptModule const&) = delete;
    ScriptModule& operator=(ScriptModule&& right) = delete;

    static std::optional<std::shared_ptr<ScriptModule>> CreateFromPath(fs::path const& path, std::optional<fs::path> cache_path);

    [[nodiscard]] char const* GetScriptRevisionHash() const
    {
        return _getScriptModuleRevisionHash();
    }

    void AddScripts() const
    {
        _addScripts();
    }

    [[nodiscard]] char const* GetScriptName() const
    {
        return _getScriptName();
    }

    [[nodiscard]] fs::path const& GetModulePath() const
    {
        return _path;
    }

private:
    HandleHolder _handle;

    GetScriptRevisionHashType _getScriptModuleRevisionHash;
    AddScriptsType _addScripts;
    GetScriptNameType _getScriptName;
    fs::path _path;
};

template<typename Fn>
static bool GetFunctionFromSharedLibrary(HandleType handle, std::string const& name, Fn& fn)
{
#if WARHEAD_PLATFORM == WARHEAD_PLATFORM_WINDOWS
    fn = reinterpret_cast<Fn>(GetProcAddress(handle, name.c_str()));
#else // Posix
    fn = reinterpret_cast<Fn>(dlsym(handle, name.c_str()));
#endif
    return fn != nullptr;
}

// Load a shared library from the given path.
std::optional<std::shared_ptr<ScriptModule>>
ScriptModule::CreateFromPath(fs::path const& path, std::optional<fs::path> cache_path)
{
    auto const load_path = [&]() -> fs::path
    {
        if (cache_path)
            return *cache_path;
        else
            return path;
    }();

#if WARHEAD_PLATFORM == WARHEAD_PLATFORM_WINDOWS
    HandleType handle = LoadLibrary(load_path.generic_string().c_str());
#else // Posix
    HandleType handle = dlopen(load_path.generic_string().c_str(), RTLD_LAZY);
#endif

    if (!handle)
    {
        if (cache_path)
        {
            LOG_ERROR("scripts.hotswap", "Could not dynamic load the shared library: {} (the library is cached at {})",
                      path.generic_string(), cache_path->generic_string());
        }
        else
        {
            LOG_ERROR("scripts.hotswap", "Could not dynamic load the shared library: {}.",
                      path.generic_string());
        }

        return {};
    }

    // Use RAII to release the library on failure.
    HandleHolder holder(handle, SharedLibraryUnloader(path, std::move(cache_path)));

    GetScriptRevisionHashType getScriptRevisionHash;
    AddScriptsType addScripts;
    GetScriptNameType getScriptName;

    if (!GetFunctionFromSharedLibrary(handle, "GetScriptRevisionHash", getScriptRevisionHash))
    {
        LOG_ERROR("scripts.hotswap", "Could not extract 'GetScriptModuleRevisionHash' function from library: {}.", load_path.generic_string());
        return {};
    }

    if (!GetFunctionFromSharedLibrary(handle, "AddScripts", addScripts))
    {
        LOG_ERROR("scripts.hotswap", "Could not extract 'AddScripts' function from library: {}.", load_path.generic_string());
        return {};
    }

    if (!GetFunctionFromSharedLibrary(handle, "GetScriptName", getScriptName))
    {
        LOG_ERROR("scripts.hotswap", "Could not extract 'GetScriptName' function from library: {}.", load_path.generic_string());
        return {};
    }

    // Unload the module at the next update tick as soon as all references are removed
    return std::make_shared<ScriptModule>(std::move(holder), getScriptRevisionHash, addScripts, getScriptName, path);
}

/*static*/ DllMgr* DllMgr::instance()
{
    static DllMgr instance;
    return &instance;
}

void DllMgr::TestDll(std::string_view dllPath)
{
    fs::path pathToDll(dllPath);
    if (pathToDll.empty())
    {
        LOG_ERROR("dll", "Empty dll path!");
        return;
    }

    if (!fs::exists(pathToDll))
    {
        LOG_ERROR("dll", "Not found file: {}", pathToDll.generic_string());
        return;
    }

    auto dllScript = ScriptModule::CreateFromPath(pathToDll, {});
    if (!dllScript)
    {
        LOG_ERROR("dll", "Can't load dll: {}", pathToDll.generic_string());
        return;
    }

    auto dllInvoke = *dllScript;

    LOG_INFO("dll", "Dll info:");
    // LOG_INFO("dll", "Name: {}", dllInvoke->GetScriptName());
    LOG_INFO("dll", "Path: {}", pathToDll.generic_string());
    LOG_INFO("dll", "Hash: {}", dllInvoke->GetScriptRevisionHash());
    (*dllScript)->AddScripts();
}
