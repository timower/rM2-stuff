url=$(curl --header "Memfault-Project-Key: JLPsMk3iNn9rmBptmhVq2NYtO4RaiFM7" "https://device.cloud.remarkable.com/api/v0/releases/latest?hardware_version=reMarkable2&software_type=device" | jq -r '.artifacts[0].url')
echo "Downloading $url"
curl -o $out "$url"
