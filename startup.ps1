# Add the directories containing the xpaset and ds9 binaries to the path
$env:path += ';c:/ds9'

# Launch ds9 if necessary
$running = xpaaccess -n Online_Preview
if ($running -eq "0") {
    & ds9 -title Online_Preview
}