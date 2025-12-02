#!/usr/bin/env bash
set -e

# Must be run from root (ImperialEngine3/)
if [ $# -ne 1 ]; then
    echo "Usage: ./setup_vscode.sh <project-name>"
    exit 1
fi

PROJECT_NAME="$1"
ROOT_DIR="$(pwd)"
PROJECT_DIR="$ROOT_DIR/projects/$PROJECT_NAME"
VSCODE_DIR="$ROOT_DIR/.vscode"

if [ ! -d "$PROJECT_DIR" ]; then
    echo "Error: Project '$PROJECT_NAME' does not exist in projects/"
    exit 1
fi

echo "Setting up VSCode for project: $PROJECT_NAME"

mkdir -p "$VSCODE_DIR"

###########################################
# 1) c_cpp_properties.json
###########################################
cat > "$VSCODE_DIR/c_cpp_properties.json" << EOF
{
    "version": 4,
    "configurations": [
        {
            "name": "$PROJECT_NAME",
            "compileCommands": "\${workspaceFolder}/projects/$PROJECT_NAME/build/compile_commands.json",
            "includePath": [
                "\${workspaceFolder}/engine/src",
                "\${workspaceFolder}/external/**",
                "\${workspaceFolder}/projects/$PROJECT_NAME/src"
            ],
            "defines": [],
            "windowsSdkVersion": "10.0.22621.0"
        }
    ]
}
EOF

###########################################
# 2) tasks.json (runs build.sh before debugging)
###########################################
cat > "$VSCODE_DIR/tasks.json" << EOF
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "build-$PROJECT_NAME",
            "type": "shell",
			      "command": "./projects/triangle/build.sh",
            "group": {
                "kind": "build",
                "isDefault": true
            },
            "problemMatcher": ["\$gcc"]
        }
    ]
}
EOF

###########################################
# 3) launch.json (MSVC debugger + preLaunchTask)
###########################################
cat > "$VSCODE_DIR/launch.json" << EOF
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Debug: $PROJECT_NAME",
            "type": "cppvsdbg",
            "request": "launch",
            "program": "\${workspaceFolder}/projects/$PROJECT_NAME/build/$PROJECT_NAME.exe",
            "cwd": "\${workspaceFolder}",
            "args": [],
            "stopAtEntry": false,
            "preLaunchTask": "build-$PROJECT_NAME",
            "console": "integratedTerminal"
        }
    ]
}
EOF

echo "VSCode setup complete for project: $PROJECT_NAME"
