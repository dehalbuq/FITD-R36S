#!/usr/bin/env bash
# Register a separate FITD runner, preserving the existing SMW runner.
set -Eeuo pipefail
[ "$(id -u)" -ne 0 ] || { echo "Run as smwrunner: sudo -iu smwrunner" >&2; exit 2; }
for x in python3 curl tar sha256sum; do command -v "$x" >/dev/null || { echo "Missing: $x" >&2; exit 3; }; done
DEST="$HOME/actions-runner-fitd-lakka"
mkdir -p "$DEST"
cd "$DEST"
[ ! -f .runner ] || { echo "FITD runner already registered in this directory." >&2; exit 4; }
echo 'Fetching current official GitHub runner release for Linux x64...'
mapfile -t RELEASE < <(python3 - <<'PY'
import json,urllib.request
req=urllib.request.Request('https://api.github.com/repos/actions/runner/releases/latest',headers={'User-Agent':'FITD-Lakka-Kali-Setup'})
with urllib.request.urlopen(req,timeout=25) as response:
    data=json.load(response)
asset=next((a for a in data['assets'] if a['name'].startswith('actions-runner-linux-x64-') and a['name'].endswith('.tar.gz')),None)
if not asset: raise SystemExit('Official Linux x64 runner archive missing')
print(asset['browser_download_url'])
print(asset.get('digest') or 'unknown')
PY
)
[ "${#RELEASE[@]}" -eq 2 ] || { echo "GitHub release not found" >&2; exit 5; }
URL="${RELEASE[0]}"
DIGEST="${RELEASE[1]}"
curl -fsSL --retry 3 "$URL" -o runner.tar.gz
if [[ "$DIGEST" == sha256:* ]]; then
  printf '%s  %s\n' "${DIGEST#sha256:}" runner.tar.gz | sha256sum -c -
else
  echo 'GitHub API did not supply a checksum. Use official manual installation instead.' >&2
  exit 6
fi
tar -xzf runner.tar.gz
rm -f runner.tar.gz
echo 'Visit: https://github.com/dehalbuq/FITD-R36S/settings/actions/runners/new'
echo 'Paste the registration token ONLY in this Kali terminal. Never share the token.'
read -rsp 'Registration token: ' TOKEN
printf '\n'
[ -n "$TOKEN" ] || { echo 'Missing token' >&2; exit 7; }
./config.sh --url https://github.com/dehalbuq/FITD-R36S --token "$TOKEN" \
  --name "kali-fitd-lakka-$(hostname)" --labels fitd-lakka-kali \
  --work _work --unattended
unset TOKEN
echo 'Runner registered. As an admin run the service installation outside this shell:'
echo "sudo $DEST/svc.sh install $(id -un)"
echo "sudo $DEST/svc.sh start"
echo "sudo $DEST/svc.sh status"
