#!/bin/bash

compile_shader() {
    local file="$1"
    local shader_type="$2"
    local relative_path="${file#"$PWD"/}"  # Get relative path from current directory
    local output="${relative_path%.*}.$shader_type.spv"
    glslc "$file" -o "$output"
    echo "Compiled $relative_path to $output"
}

find_shaders() {
    local dir="$1"
    shopt -s nullglob
    
	for file in "$dir"/*.comp; do
        if [ -f "$file" ]; then
            compile_shader "$file" "comp"
        fi
    done
    for file in "$dir"/*.vert; do
        if [ -f "$file" ]; then
            compile_shader "$file" "vert"
        fi
    done
    for file in "$dir"/*.frag; do
        if [ -f "$file" ]; then
            compile_shader "$file" "frag"
        fi
    done
	
    for subdir in "$dir"/*/; do
        find_shaders "$subdir"
    done
}

# Start compilation from the current directory
find_shaders "$PWD"

# Wait for user input before exiting
read -n 1 -s -r -p "Press any key to continue..."