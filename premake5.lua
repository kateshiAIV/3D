workspace "3D"
    architecture "x64"

    configurations
    {
        "Debug",
        "Release"
    }

    startproject "3D"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

IncludeDir = {}
IncludeDir["GLFW"] = "3D/vendor/GLFW/include"
IncludeDir["STB"] = "3D/vendor/stb"

include "3D/vendor/GLFW"

project "3D"
    location "3D"
    kind "ConsoleApp"
    language "C++"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "3D/src/**.cpp"
    }

    includedirs
    {
        IncludeDir["GLFW"],
        IncludeDir["STB"]
    }

    links
    {
        "GLFW",
        "opengl32.lib"
    }

    filter "system:windows"
        cppdialect "C++23"
        systemversion "latest"

    filter "configurations:Debug"
        runtime "Debug"
        symbols "On"

    filter "configurations:Release"
        runtime "Release"
        optimize "On"

    filter {}