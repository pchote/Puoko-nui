$running = C:\ds9\xpaaccess -n Online_Preview
if ($running -ne "0") {
    & C:\ds9\xpaset -p Online_Preview file preview.fits.gz
}