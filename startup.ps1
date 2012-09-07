# Add the directories containing the xpaset and ds9 binaries to the path
$env:path += ';c:/ds9'

# Start the XPA server in the background.
# It will automatically terminate if another
# instance is running, so this is safe.
Start-Job -scriptblock { & xpans }

# Launch ds9 if necessary
$running = xpaaccess -n Online_Preview
if ($running -eq "0") {
    & ds9 -title Online_Preview
}