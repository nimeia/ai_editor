# V1 鏋勫缓杩愯涓庢祴璇曟寚鍗?
## 1. 鏋勫缓

### POSIX

```bash
cmake -S . -B build
cmake --build build -j1
```

### Windows

```powershell
cmake -S . -B build
cmake --build build --config Release
```

## 2. 杩愯娴嬭瘯

### POSIX 鍏ㄩ噺娴嬭瘯

```bash
ctest --test-dir build --output-on-failure
```

### Windows 鍏ㄩ噺娴嬭瘯

```powershell
ctest --test-dir build -C Release --output-on-failure
```


### POSIX 鍔熻兘娴嬭瘯鑴氭湰锛堟寜鍔熻兘鍩燂級

```bash
ctest --test-dir build -R 'test_functional_(workspace_ops|fs_ops|search_ops|patch_lifecycle|stream_cancel_timeout|logging_release)' --output-on-failure
```

褰撳墠宸茶ˉ榻愮殑鍔熻兘鑴氭湰鍒嗙粍锛?
- workspace 鍩虹閾捐矾锛歚test_functional_workspace_ops`
- 鏂囦欢璇诲彇涓庤竟鐣岋細`test_functional_fs_ops`
- 鎼滅储涓庤繃婊わ細`test_functional_search_ops`
- patch 鐢熷懡鍛ㄦ湡锛歚test_functional_patch_lifecycle`
- stream / cancel / timeout锛歚test_functional_stream_cancel_timeout`
- runtime/audit 鏃ュ織 + install/cpack 楠岃瘉锛歚test_functional_logging_release`

### Windows smoke

```powershell
pwsh ./scripts/windows_smoke.ps1 -BuildDir ./build -Config Release
```

## 3. 鎵嬪伐杩愯

### 鍚姩 daemon

POSIX:

```bash
./build/apps/bridge_daemon/bridge_daemon --workspace "$PWD"
```

Windows:

```powershell
.\build\apps\bridge_daemon\Release\bridge_daemon.exe --workspace $PWD
```

### 甯哥敤 CLI 鍛戒护

POSIX:

```bash
./build/apps/bridge_cli/bridge_cli ping --workspace "$PWD"
./build/apps/bridge_cli/bridge_cli info --workspace "$PWD"
./build/apps/bridge_cli/bridge_cli list --workspace "$PWD"
./build/apps/bridge_cli/bridge_cli read --workspace "$PWD" --path README.md
./build/apps/bridge_cli/bridge_cli search-text --workspace "$PWD" --query bridge --exts .md,.cpp
```

Windows:

```powershell
.\build\apps\bridge_cli\Release\bridge_cli.exe ping --workspace $PWD
.\build\apps\bridge_cli\Release\bridge_cli.exe info --workspace $PWD
.\build\apps\bridge_cli\Release\bridge_cli.exe list --workspace $PWD
.\build\apps\bridge_cli\Release\bridge_cli.exe read --workspace $PWD --path README.md
.\build\apps\bridge_cli\Release\bridge_cli.exe search-text --workspace $PWD --query bridge --exts .md,.cpp
```

## 4. 閲嶇偣楠岃瘉椤?
- handshake / ping / info / open
- workspace 璺緞褰掍竴鍖栦笌 containment
- list / stat / read / read-range
- search text / regex
- stream final summary 瀛楁缁熶竴
- timeout / cancel 琛屼负
- patch preview / apply / rollback / history
- Windows unicode / space path 涓?backslash-path normalization
- runtime / audit / history / preview / backup 钀界洏涓庢竻鐞?
## 5. 璇婃柇寤鸿

### 鐪?runtime 璺緞

浼樺厛閫氳繃 `workspace.info` 鎴?`workspace.open` 杩斿洖鐨?`runtime_dir` 鍋氬畾浣嶏紝涓嶈鍦ㄨ剼鏈噷鍐欐杩愯鏃剁洰褰曘€?
### 鐪嬫棩蹇?
- runtime log锛氬畾浣?daemon 杩愯鏃堕敊璇?- audit log锛氬畾浣嶈姹傜骇琛屼负鍜岄敊璇爜
- history log锛氬畾浣?patch/rollback 閾捐矾

### 鍏虫敞 timeout / cancel

褰撳墠鑻ヤ娇鐢?`--timeout-ms`锛孋LI 浼氱粰 transport 澶氱暀涓€涓皬缂撳啿锛岃繖鏍锋洿瀹规槗鎷垮埌缁撴瀯鍖?`REQUEST_TIMEOUT` / `SEARCH_TIMEOUT` 鍝嶅簲锛岃€屼笉鏄洿鎺ヨ繛鎺ヨ秴鏃躲€?

## 鍙戝竷鎵撳寘

P5 宸茶ˉ榻愬畨瑁呬笌褰掓。鎵撳寘鍩虹嚎锛孭6 鍙堣ˉ浜嗕竴閿叏閲忛獙璇佽剼鏈€?
- POSIX 鎵撳寘锛歚./scripts/package_release.sh --build-dir build --out-dir dist --generator TGZ --run-tests --jobs 1`
- Windows 鎵撳寘锛歚pwsh ./scripts/package_release.ps1 -BuildDir build -Config Release -OutDir dist -Generator ZIP -RunTests -Jobs 1`
- POSIX 鍏ㄩ噺楠岃瘉锛歚./scripts/validate_v1.sh --build-dir build --jobs 1`
- Windows 鍘熺敓鍔熻兘娴嬭瘯锛歚pwsh ./tests/functional_windows_native.ps1 <daemon.exe> <bridge_cli.exe> <run_dir>`
- Windows 鍏ㄩ噺楠岃瘉锛歚pwsh ./scripts/validate_v1.ps1 -BuildDir build -Config Release -Jobs 1`

Windows 鍏ㄩ噺楠岃瘉鑴氭湰鐜板湪浼氬湪 `.p6_validation_summary/` 涓嬭嚜鍔ㄧ敓鎴愶細

- `windows_validation_summary.json`锛氱粨鏋勫寲鏈哄櫒鍙鎽樿
- `windows_validation_summary.md`锛氫究浜庝汉宸ユ煡鐪嬬殑闃舵鎬х粨鏋滄眹鎬?
鍗充娇涓€旀煇涓樁娈靛け璐ワ紝涓婅堪鎽樿鏂囦欢涔熶細鍐欏嚭锛屼究浜庡揩閫熷畾浣嶅け璐ユ楠ゃ€?
鍦ㄨ祫婧愬彈闄愮幆澧冮噷锛屽缓璁紭鍏堜娇鐢?`jobs=1`锛屼互鎹㈠彇鏇寸ǔ瀹氱殑鏀跺熬涓庢墦鍖呰繃绋嬨€?
鏇村畬鏁寸殑鍙戝竷璇存槑瑙?`docs/08-v1-release-and-deployment.md`锛屾湰娆￠獙璇佺粨鏋滆 `docs/09-v1-validation-report.md`銆?

## 7. GitHub Actions

浠撳簱鎻愪緵浜嗕袱鏉?GitHub Actions 宸ヤ綔娴侊細

- `ci.yml`锛氭棩甯?CI锛岃鐩?Linux / macOS / Windows
- `release.yml`锛氭爣绛惧彂甯冧笌鎵嬪伐鍙戝竷锛岃鐩?Linux / macOS / Windows锛屽苟涓婁紶 Release 浜х墿

杩欎袱鏉″伐浣滄祦閮界洿鎺ヨ皟鐢ㄤ粨搴撳唴鐜版湁鑴氭湰锛?
- POSIX锛歚./scripts/validate_v1.sh --build-dir build --config Release --jobs 1`
- Windows锛歚pwsh ./scripts/validate_v1.ps1 -BuildDir build -Config Release -Jobs 1`

杩欐牱鏈湴楠岃瘉璺緞涓?CI 璺緞淇濇寔涓€鑷达紝鍑忓皯鈥滄湰鍦拌兘杩囥€丆I 涓嶈繃鈥濈殑鍒嗗弶闂銆?