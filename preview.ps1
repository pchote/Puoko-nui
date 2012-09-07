# Add the directories containing the xpaset and ds9 binaries to the path
$env:path += ';c:/ds9'

# Preview is stored in the working dir, but xpaset requires forward-slash path separators
$previewdir = $PWD -replace '\\','/'

# Only update the preview if the appropriate ds9 window exists
$running = xpaaccess -n Online_Preview
if ($running -eq "0") {
    & ds9 -title Online_Preview
} else {
    & xpaset -p Online_Preview file "$previewdir/preview.fits.gz"
}