#!/bin/bash -e

echo "Linking sources..."

mkdir -p .intermediate/sources

travel() {
	local travelDir=$1
	local relativeDir=$2
	local targetDir=$3
	
	for file in "$travelDir"/*; do
		local target=$targetDir/$file
			
		if [[ -d "$file" ]]; then
			mkdir -p "$target"
			travel "$file" "$relativeDir/.." "$targetDir"
	    elif [[ -f "$file" && ! -h "$target" ]]; then
	        ln -sf "$relativeDir/$file" "$target"
	    fi
	done
}

travel sources ../.. .intermediate
