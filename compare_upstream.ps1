param(
    [string]$UpstreamDir = "D:\3sxtra\tmp\upstream_3sx\src",
    [string]$ForkDir = "D:\3sxtra\src"
)

Write-Host "Finding files that exist in both but differ..."
$upstreamFiles = Get-ChildItem -Path $UpstreamDir -Recurse -File
$count = 0

foreach ($file in $upstreamFiles) {
    # Compute relative path
    $relPath = $file.FullName.Substring($UpstreamDir.Length + 1)
    $forkFile = Join-Path $ForkDir $relPath

    if (Test-Path $forkFile) {
        # Compare file contents simply by hash or size/time, but since we know they differ let's use git diff
        $diff = git diff --no-index --shortstat $file.FullName $forkFile
        if ($diff) {
            Write-Host "Differences found in: $relPath"
            # code -d $file.FullName $forkFile
            # We can prompt the user to open it
            $count++
        }
    }
}

Write-Host "Total differing files found: $count"
Write-Host "Run 'code -d D:\3sxtra\tmp\upstream_3sx\src\<file> D:\3sxtra\src\<file>' to view a specific diff in VS Code."
