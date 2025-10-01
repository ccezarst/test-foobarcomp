Param(
    [Parameter(Mandatory = $true)]
    [string]$KeyId,

    [Parameter(Mandatory = $true)]
    [string]$TeamId,

    [Parameter(Mandatory = $true)]
    [string]$PrivateKeyPath
)

$now = [DateTimeOffset]::UtcNow
$exp = $now.AddHours(12)

$payload = @{ 
    iss = $TeamId
    exp = $exp.ToUnixTimeSeconds()
    iat = $now.ToUnixTimeSeconds()
}

$header = @{ 
    alg = "ES256"
    kid = $KeyId
    typ = "JWT"
}

$headerJson = $header | ConvertTo-Json -Compress
$payloadJson = $payload | ConvertTo-Json -Compress

function Base64UrlEncode([byte[]]$bytes) {
    return [Convert]::ToBase64String($bytes).TrimEnd('=').Replace('+', '-').Replace('/', '_')
}

$headerEncoded = Base64UrlEncode([System.Text.Encoding]::UTF8.GetBytes($headerJson))
$payloadEncoded = Base64UrlEncode([System.Text.Encoding]::UTF8.GetBytes($payloadJson))

$unsignedToken = "$headerEncoded.$payloadEncoded"

$privateKey = Get-Content -Path $PrivateKeyPath -Raw
$privateKeyBytes = [System.Convert]::FromBase64String(($privateKey -replace "-----\w+ PRIVATE KEY-----", "" -replace "\s", ""))

$signature = [System.Security.Cryptography.SignatureAlgorithm]::Create("SHA256withECDSA")
$ecdsa = [System.Security.Cryptography.ECDsa]::Create()
$ecdsa.ImportPkcs8PrivateKey($privateKeyBytes, [ref]0) | Out-Null
$signedBytes = $ecdsa.SignData([System.Text.Encoding]::UTF8.GetBytes($unsignedToken), [System.Security.Cryptography.HashAlgorithmName]::SHA256)

$token = "$unsignedToken.$(Base64UrlEncode($signedBytes))"

Write-Output "Developer Token: $token"
