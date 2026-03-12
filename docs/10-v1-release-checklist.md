# V1 鏈€缁堝彂甯冩鏌ユ竻鍗?

## 1. 婧愮爜涓庢枃妗?

- [ ] 鐗堟湰鍙风‘璁ゆ棤璇?
- [ ] `README.md` 涓庡綋鍓嶅疄鐜颁竴鑷?
- [ ] `docs/04-v1-protocol.md` 涓庡綋鍓嶆柟娉?閿欒鐮佷竴鑷?
- [ ] `docs/07-v1-build-run-test-guide.md` 鍙洿鎺ユ寚瀵兼瀯寤轰笌楠岃瘉
- [ ] `docs/08-v1-release-and-deployment.md` 鍙洿鎺ユ寚瀵兼墦鍖呬笌閮ㄧ讲
- [ ] `docs/09-v1-validation-report.md` 宸叉洿鏂颁负鏈鍊欓€夌増鏈殑瀹為檯缁撴灉

## 2. Linux / POSIX 楠岃瘉

- [ ] `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`
- [ ] `cmake --build build --parallel 1` 鎴栧叾浠栫‘璁ょǔ瀹氱殑骞惰搴?
- [ ] `ctest --test-dir build --output-on-failure`
- [ ] `./scripts/validate_v1.sh --build-dir build --jobs 1`
- [ ] 鎵嬪伐鏌ョ湅 `.bridge/audit.log` 涓?`.bridge/history.log`

## 3. Windows 楠岃瘉

- [ ] `cmake -S . -B build -G "Visual Studio 17 2022" -A x64`
- [ ] `cmake --build build --config Release --parallel 1` 鎴栧叾浠栫‘璁ょǔ瀹氱殑骞惰搴?
- [ ] `ctest --test-dir build -C Release --output-on-failure`
- [ ] `pwsh ./scripts/windows_smoke.ps1 -BuildDir ./build -Config Release`
- [ ] `pwsh ./scripts/validate_v1.ps1 -BuildDir build -Config Release -Jobs 1`
- [ ] 妫€鏌?`.p6_validation_summary/windows_validation_summary.json` 涓墍鏈夐樁娈?`ok=true`
- [ ] 妫€鏌?`.p6_validation_summary/windows_validation_summary.md` 涓?install checks / artifacts 鎽樿

## 4. 瀹夎涓庡綊妗?

- [ ] `cmake --install build --prefix install-root`
- [ ] 瀹夎鏍戝唴瀛樺湪 `bin/bridge_daemon` 涓?`bin/bridge_cli`
- [ ] 瀹夎鏍戝唴瀛樺湪 `share/ai_bridge/docs/*`
- [ ] POSIX 褰掓。锛歚./scripts/package_release.sh --build-dir build --out-dir dist --generator TGZ --jobs 1`
- [ ] Windows 褰掓。锛歚pwsh ./scripts/package_release.ps1 -BuildDir build -Config Release -OutDir dist -Generator ZIP -Jobs 1`
- [ ] 鏈湴鎵撳寘杈撳嚭宸茬敓鎴?`SHA256SUMS.txt` 骞舵牎楠?
- [ ] GitHub Release 璧勪骇涓殑骞冲彴鏍￠獙鏂囦欢鍚嶆棤鍐茬獊锛堝 `SHA256SUMS-linux.txt` / `SHA256SUMS-windows.txt`锛?

## 5. 鏍稿績閾捐矾鎶芥煡

- [ ] `ping`
- [ ] `info`
- [ ] `open`
- [ ] `list`
- [ ] `stat`
- [ ] `read`
- [ ] `read-range`
- [ ] `search-text`
- [ ] `search-regex`
- [ ] `read --stream`
- [ ] `patch-preview`
- [ ] `patch-apply`
- [ ] `history`
- [ ] `patch-rollback`
- [ ] `cancel`
- [ ] `timeout`

## 6. 鍙戝竷鍐崇瓥

- [ ] 鏈鍊欓€夌増鏈病鏈夐樆濉炵骇 crash / 鏁版嵁鎹熷潖闂
- [ ] release artifact 宸插綊妗ｄ繚瀛?
- [ ] 鎵嬪伐瑙﹀彂 release 鏃讹紝宸茬‘璁?workflow checkout 鐨勬槸鐩爣 tag/ref
- [ ] 鍝堝笇鏂囦欢宸插綊妗ｄ繚瀛?
- [ ] 宸茶褰?Windows 涓?Linux 鐨勬渶缁堥獙璇佺粨鏋?
- [ ] 鍏佽杩涘叆 V1 鍙戝竷
