# output.ps1
$projectPath = Get-Location
$skipDirs = @(".git", "dist", "build", ".vscode")
$skipExts = @("jpg", "webp", "jpeg", "png", "gif", "bmp", "tiff", "ico", "svg", "psd", "eps", "raw", "heif", "heic", "ps1", "sh", "gitignore")

function Get-FileContent {
    param ([string]$path)

    $items = Get-ChildItem -Path $path

    foreach ($item in $items) {
        if ($item.PSIsContainer) {
            if ($item.Name -notin $skipDirs) {
                Write-Output "Directory: $($item.FullName)"
                Get-FileContent -path $item.FullName
            }
        } elseif ($item.Name -ne "output.txt") {
            $ext = $item.Extension.TrimStart('.')
            if ($ext -notin $skipExts) {
                Write-Output "$($item.Name)"
                $fileContent = Get-Content -Path $item.FullName -Raw
                Write-Output "`n````$ext`n$fileContent`n````"
            }
        }
    }
}

Get-FileContent -path $projectPath > output.txt