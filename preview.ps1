$running = C:\ds9\xpaaccess -n Online_Preview
if ($running -eq "0") {
    & C:\ds9\ds9 -title Online_Preview
} else {
    & C:\ds9\xpaset -p Online_Preview file preview.fits.gz
}