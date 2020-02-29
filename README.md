# presentation_remote_cmd

## 始めに
これらのプログラムはこれらのページを参考にして作成しました(ほとんどパクリのようなものです)
- http://kako.com/blog/?p=34685
- https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering
- https://ameblo.jp/akihiko/entry-10030661458.html


## 開発・実行環境
- Windows 10 home (1903)
- Microsoft(R) C/C++ Optimizing Compiler Version 19.24.28316


## hid_dll.c
kakoさん作のtiny_hid_dllをまねて作ったものです。
時々、2分の1の確率でhandleをとって来れない時間帯が生まれます


## joycon_presentation_remote.cpp
kakoさん作のjoycon_test_v02.cを参考にして作ったものです。


## ビルド方法
```
cl hid_lib.c /LD
cl joycon_presentation_remote.cpp user32.lib hid_lib.lib /MD /D_AFXDLL
```

## 操作方法
- 方向ボタン -> 方向キー
- SR + SL + マイナス -> ESCキー
- SR + SL + L + ZL -> joycon_presentation_remoteの終了


## その他
- 一応exeとdllも入れてあります。
- 何か問題があったら消します。
