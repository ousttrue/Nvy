# Nvy custom

[Nvy](./README_original.md)

```
+-frontend-+                  +-nvim-+
|HWND      |nvim_ui_attach    |      |
| keyboard |nvim_input        |      |
| mouse    |----------------->|      |
|Grid      |nvim_ui_try_resize|      |
| rows,cols|----------------->|      |
|          |            redraw|      |
| +---------<-----------------|      |
+-|--------+                  +------+
  |      ^ RectSize
  V      | FontSize
+---------------+
|bitmap renderer|
+---------------+
```

## nvim api

<https://neovim.io/doc/user/api.html>

`> nvim --embed`

### message sequence

```
[0,0,"nvim_get_api_info",[]] =>
[2,"nvim_set_var",["nvy",1]] =>
[0,1,"nvim_eval",["stdpath('config')"]] =>
<= [response#0]
<= [response#1]
[2,"nvim_ui_attach",[190,45,{"ext_linegrid":true}]] =>
[2,"nvim_ui_try_resize",[190,45]] => 
<= [notify, redraw]
```
