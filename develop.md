## install

```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "Configure AD_4CH Debug",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=AD_4CH",
        "-DBUILD_TYPE=Debug",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure AD_4CH Debug"
    },
    {
      "label": "Configure AD_4CH Release",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=AD_4CH",
        "-DBUILD_TYPE=Release",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure AD_4CH Release"
    },
    {
      "label": "Configure CMP_4CH Debug",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=CMP_4CH",
        "-DBUILD_TYPE=Debug",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure CMP_4CH Debug"
    },
    {
      "label": "Configure CMP_4CH Release",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=CMP_4CH",
        "-DBUILD_TYPE=Release",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure CMP_4CH Release"
    },
    {
      "label": "Configure DA_4CH Debug",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=DA_4CH",
        "-DBUILD_TYPE=Debug",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure DA_4CH Debug"
    },
    {
      "label": "Configure DA_4CH Release",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=DA_4CH",
        "-DBUILD_TYPE=Release",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure DA_4CH Release"
    },
    {
      "label": "Configure DIG_16IN_16OUT_NPN Debug",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=DIG_16IN_16OUT_NPN",
        "-DBUILD_TYPE=Debug",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure DIG_16IN_16OUT_NPN Debug"
    },
    {
      "label": "Configure DIG_16IN_16OUT_NPN Release",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=DIG_16IN_16OUT_NPN",
        "-DBUILD_TYPE=Release",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure DIG_16IN_16OUT_NPN Release"
    },
    {
      "label": "Configure DIG_16IN_16OUT_PNP Debug",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=DIG_16IN_16OUT_PNP",
        "-DBUILD_TYPE=Debug",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure DIG_16IN_16OUT_PNP Debug"
    },
    {
      "label": "Configure DIG_16IN_16OUT_PNP Release",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=DIG_16IN_16OUT_PNP",
        "-DBUILD_TYPE=Release",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure DIG_16IN_16OUT_PNP Release"
    },
    {
      "label": "Configure DIG_16IN_NPN Debug",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=DIG_16IN_NPN",
        "-DBUILD_TYPE=Debug",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure DIG_16IN_NPN Debug"
    },
    {
      "label": "Configure DIG_16IN_NPN Release",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=DIG_16IN_NPN",
        "-DBUILD_TYPE=Release",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure DIG_16IN_NPN Release"
    },
    {
      "label": "Configure DIG_16IN_PNP Debug",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=DIG_16IN_PNP",
        "-DBUILD_TYPE=Debug",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure DIG_16IN_PNP Debug"
    },
    {
      "label": "Configure DIG_16IN_PNP Release",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=DIG_16IN_PNP",
        "-DBUILD_TYPE=Release",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure DIG_16IN_PNP Release"
    },
    {
      "label": "Configure DIG_16OUT_NPN Debug",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=DIG_16OUT_NPN",
        "-DBUILD_TYPE=Debug",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure DIG_16OUT_NPN Debug"
    },
    {
      "label": "Configure DIG_16OUT_NPN Release",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=DIG_16OUT_NPN",
        "-DBUILD_TYPE=Release",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure DIG_16OUT_NPN Release"
    },
    {
      "label": "Configure DIG_16OUT_PNP Debug",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=DIG_16OUT_PNP",
        "-DBUILD_TYPE=Debug",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure DIG_16OUT_PNP Debug"
    },
    {
      "label": "Configure DIG_16OUT_PNP Release",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=DIG_16OUT_PNP",
        "-DBUILD_TYPE=Release",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure DIG_16OUT_PNP Release"
    },
    {
      "label": "Configure DIG_32IN_NPN Debug",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=DIG_32IN_NPN",
        "-DBUILD_TYPE=Debug",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure DIG_32IN_NPN Debug"
    },
    {
      "label": "Configure DIG_32IN_NPN Release",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=DIG_32IN_NPN",
        "-DBUILD_TYPE=Release",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure DIG_32IN_NPN Release"
    },
    {
      "label": "Configure DIG_32IN_PNP Debug",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=DIG_32IN_PNP",
        "-DBUILD_TYPE=Debug",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure DIG_32IN_PNP Debug"
    },
    {
      "label": "Configure DIG_32IN_PNP Release",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=DIG_32IN_PNP",
        "-DBUILD_TYPE=Release",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure DIG_32IN_PNP Release"
    },
    {
      "label": "Configure DIG_32OUT_NPN Debug",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=DIG_32OUT_NPN",
        "-DBUILD_TYPE=Debug",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure DIG_32OUT_NPN Debug"
    },
    {
      "label": "Configure DIG_32OUT_NPN Release",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=DIG_32OUT_NPN",
        "-DBUILD_TYPE=Release",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure DIG_32OUT_NPN Release"
    },
    {
      "label": "Configure DIG_32OUT_PNP Debug",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=DIG_32OUT_PNP",
        "-DBUILD_TYPE=Debug",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure DIG_32OUT_PNP Debug"
    },
    {
      "label": "Configure DIG_32OUT_PNP Release",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=DIG_32OUT_PNP",
        "-DBUILD_TYPE=Release",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure DIG_32OUT_PNP Release"
    },
    {
      "label": "Configure DIG_8IN_8OUT_NPN Debug",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=DIG_8IN_8OUT_NPN",
        "-DBUILD_TYPE=Debug",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure DIG_8IN_8OUT_NPN Debug"
    },
    {
      "label": "Configure DIG_8IN_8OUT_NPN Release",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=DIG_8IN_8OUT_NPN",
        "-DBUILD_TYPE=Release",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure DIG_8IN_8OUT_NPN Release"
    },
    {
      "label": "Configure DIG_8IN_8OUT_PNP Debug",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=DIG_8IN_8OUT_PNP",
        "-DBUILD_TYPE=Debug",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure DIG_8IN_8OUT_PNP Debug"
    },
    {
      "label": "Configure DIG_8IN_8OUT_PNP Release",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=DIG_8IN_8OUT_PNP",
        "-DBUILD_TYPE=Release",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure DIG_8IN_8OUT_PNP Release"
    },
    {
      "label": "Configure ENC_2CH Debug",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=ENC_2CH",
        "-DBUILD_TYPE=Debug",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure ENC_2CH Debug"
    },
    {
      "label": "Configure ENC_2CH Release",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=ENC_2CH",
        "-DBUILD_TYPE=Release",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure ENC_2CH Release"
    },
    {
      "label": "Configure PWM_4CH Debug",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=PWM_4CH",
        "-DBUILD_TYPE=Debug",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure PWM_4CH Debug"
    },
    {
      "label": "Configure PWM_4CH Release",
      "type": "process",
      "command": "cmake",
      "args": [
        "-DBOARD=PWM_4CH",
        "-DBUILD_TYPE=Release",
        "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc.cmake",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "build"
      ],
      "detail": "Configure PWM_4CH Release"
    },
    {
      "label": "Build Project",
      "type": "process",
      "command": "cmake",
      "args": [
        "--build",
        "build"
      ],
      "group": {
        "kind": "build",
        "isDefault": true
      },
      "problemMatcher": [
        "$gcc"
      ]
    },
    {
      "label": "Clean Project",
      "type": "process",
      "command": "cmake",
      "args": [
        "--build",
        "build",
        "--target",
        "clean"
      ],
      "problemMatcher": [
        "$gcc"
      ]
    },
    {
      "label": "Clean & Rebuild",
      "dependsOrder": "sequence",
      "dependsOn": [
        "Clean Project",
        "Build Project"
      ],
      "problemMatcher": [
        "$gcc"
      ]
    }
  ]
}
```
