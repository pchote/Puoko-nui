# Add the directory containing tsreduce to the path
$env:path += ';c:/MinGW/msys/home/ccd/tsreduce'

# Change this line to point at the reduction data file
$dir = (Get-ChildItem $args).DirectoryName
$dir_fs = $dir -replace '\\','/'
$file = "$dir_fs\undefined.dat"

# Uncomment the following lines to enable online reduction
# & tsreduce update $file
# & tsreduce plot $file online_ts.gif/gif online_dft.gif/gif
