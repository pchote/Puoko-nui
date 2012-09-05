$running = C:\ds9\xpaaccess -n Online_Preview
if ($running -eq "0") {
    & C:\ds9\ds9 -title Online_Preview
}