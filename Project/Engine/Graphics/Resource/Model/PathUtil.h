#pragma once
#include <string>
#include <cstring>

namespace K4E::PathUtil
{
    inline std::string NormalizeSlashes(std::string path)
    {
        for (char& c : path)
        {
            if (c == '\\')
            {
                c = '/';
            }
        }
        return path;
    }

    inline bool StartsWith(const std::string& s, const std::string& prefix)
    {
        return s.rfind(prefix, 0) == 0;
    }

    inline std::string ResolveModelPath(const std::string& path)
    {
        constexpr const char* kModelRoot = "Resources/Models/";

        std::string p = NormalizeSlashes(path);

        // すでに完成パスならそのまま返す
        if (StartsWith(p, kModelRoot))
        {
            return p;
        }

        // 相対パスなら Resources/Models/ を付ける
        return std::string(kModelRoot) + p;
    }

    inline std::string ToModelRelativePath(const std::string& path)
    {
        constexpr const char* kModelRoot = "Resources/Models/";

        std::string p = NormalizeSlashes(path);

        if (StartsWith(p, kModelRoot))
        {
            return p.substr(std::strlen(kModelRoot));
        }

        return p;
    }
}