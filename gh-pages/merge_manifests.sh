#! bin/bash

echo "$(echo $NAMES | jq -r '.[]')" | while read -r name; do
    sanitized_name=$(echo \"$name\" | jq -r 'sub("(\\s)"; "_"; "g")')
    echo "Processing $name"
    jq -s --arg name "$name" '[.[] | select(.name == $name)] | (.[0] | with_entries(select(.key != "builds"))) + {builds: map(.builds) | add}' ./_site/publish/*/manifest.json > ./_site/publish/${sanitized_name}_manifest.json
done