# Gcode更改

1. ![image](https://github.com/user-attachments/assets/c20b5580-4b52-4168-83f2-b6780a64c8a5)在这里选择自己的机型
   ===
2. ![image](https://github.com/user-attachments/assets/a4c3c85b-7fab-44b3-9113-058054e06b15)选择编辑打印机
   ===
3. ![image](https://github.com/user-attachments/assets/1a72f02e-4659-469f-83fd-7bf9c3d2c28e)选择gcode
   ===
4. ![image](https://github.com/user-attachments/assets/0c3d79dc-210a-427a-9011-149398944d52)找到耗材丝更换G-code
   ===
5.根据下方不同机型进行不同操作
   ===

## A1mini
全选耗材丝更换Gcode，替换为下面的：
```
; ===== machine: Bambu A1 mini =============
; ===== date: 20240613 =====================
; ===== AP AMS  删除原来全部自带的，替换新的====================

G392 S0
M1007 S0

M204 S9000
{if toolchange_count > 1}
G17
G2 Z{max_layer_z + 0.4} I0.86 J0.86 P1 F10000 ; spiral lift a little from second lift
{endif}
G1 Z{max_layer_z + 3.0} F1200
G1 X-13.5 F3000 ; back to the toilet
M400
G1 E-9 F500

; 回中防止Z轴撞顶
{if (toolchange_count+1) % 10 == 1} ; 建议每10次回一次中，可以改少点观察那位置回中
M106 P1 S220
M109 S{new_filament_temp-20}
M400 S2
G1 X-3.5 F18000
G1 X-13.5 F3000
G1 X-3.5 F18000
G1 X-13.5 F3000
G1 X-3.5 F18000
G1 X-13.5 F3000

G1 Y175 F30000.1 ;Brush material
G1 X25 F30000.1
G1 Z0.2 F30000.1
G1 Y185
G91
G1 X-30 F18000
G1 Y-2
G1 X27
G1 Y1.5
G1 X-28
G1 Y-2
G1 X30
G1 Y1.5
G1 X-30
G90
M83
G1 Z5 F3000
G1 E-3.5 F900
G0 X50 Y175 F20000 ; find a soft place to home
G28 Z P0 T300; home z with low precision, permit 300deg temperature
G29.2 S0 ; turn off ABL
G1 Z{max_layer_z + 3.0} F1200
G1 X-13.5 F3000
{endif}

M106 P1 S195
M106 P2 S30
{if (toolchange_count)== 1}
M73 P{110+next_extruder} R[next_extruder] L5
{else}
M73 P{110+next_extruder} R[next_extruder] L{10+previous_extruder}
{endif}
M400 U1
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
M106 P1 S230
M109 S[new_filament_temp]
G1 E7 F{new_filament_e_feedrate}
G92 E0
G1 E-[new_retract_length_toolchange] F1800
M400
M400 S3
G1 X-3.5 F18000
G1 X-13.5 F3000
G1 X-3.5 F18000
G1 X-13.5 F3000
G1 X-3.5 F18000
G1 X-13.5 F3000
G1 X-3.5 F18000
G1 X-13.5 F3000
M400
G1 Z{max_layer_z + 3.0} F2000
M106 P1 S0

{if layer_num < 0}
;校准挤出
G90
M83
G0 X68 Y-0 F30000
G0 Z0.3 F18000 ;Move to start position
G0 X88 E10  F{outer_wall_volumetric_speed/(24/20)    * 60}
G0 X93 E.3742  F{outer_wall_volumetric_speed/(0.3*0.5)/4     * 60}
G0 X98 E.3742  F{outer_wall_volumetric_speed/(0.3*0.5)     * 60}
G0 X103 E.3742  F{outer_wall_volumetric_speed/(0.3*0.5)/4     * 60}
G0 X108 E.3742  F{outer_wall_volumetric_speed/(0.3*0.5)     * 60}
G0 X113 E.3742  F{outer_wall_volumetric_speed/(0.3*0.5)/4     * 60}
G0 X115 Z0 F20000
G0 Z5
M400
{endif}

{if layer_z <= (initial_layer_print_height + 0.001)}
G1 X[x_after_toolchange]  Y[y_after_toolchange] F12000
G1 Z{z_after_toolchange+1} F1200
{endif}

M620 S[next_extruder]A
T[next_extruder]
M621 S[next_extruder]A
G392 S0
M1007 S1
```
## A1
全选耗材丝更换Gcode，替换为下面的：
```
;===== machine: A1 =========================
;===== date: 202409214 ======================
; ===== APAMS  删除原来全部自带的，替换新的======
M1007 S0 ; turn off mass estimation
G392 S0

M204 S9000
{if toolchange_count > 1}
G17
G2 Z{max_layer_z + 0.4} I0.86 J0.86 P1 F10000 ; spiral lift a little from second lift
{endif}
G1 Z{max_layer_z + 3.0} F1200
G1 X-48 F3000 ; back to the toilet
M400
G1 E-9 F500

; 回中防止Z轴撞顶
{if (toolchange_count+1) % 10 == 1} ; 建议每10次回一次中，可以改少点观察那位置回中
M106 P1 S220
M109 S{new_filament_temp-20}
M400 S2
G1 X-28.5 F18000
G1 X-48.2 F3000
G1 X-28.5 F18000
G1 X-48.2 F3000
G1 X-28.5 F12000
G1 X-48.2 F3000

G90
G1 Y250.000 F30000
G1 X30 F30000
G1 Z1.300 F1200
G1 Y262.5 F6000
G91
G1 X35 F30000
G1 Y-0.5
G1 X-45
G1 Y-0.5
G1 X45
G1 Y-0.5
G1 X-45
G1 Y-0.5
G1 X45
G1 Y-0.5
G1 X-45
G1 Z5.000 F1200
G90
G1 E-3.5 F900
G0 X128 F30000
G0 Y254 F3000
G28 Z P0 T300; home z with low precision, permit 300deg temperature
G29.2 S0 ; turn off ABL
G1 Z{max_layer_z + 3.0} F1200
G1 X-48 F3000
{endif}

M106 P1 S195
M106 P2 S30
{if (toolchange_count)== 1}
M73 P{110+next_extruder} R[next_extruder] L5
{else}
M73 P{110+next_extruder} R[next_extruder] L{10+previous_extruder}
{endif}
M400 U1
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
M106 P1 S230
M109 S[new_filament_temp]
G1 E7 F{new_filament_e_feedrate}
G92 E0
G1 E-[new_retract_length_toolchange] F1800
M400
M400 S3
G1 X-28.5 F18000
G1 X-48.2 F3000
G1 X-28.5 F18000
G1 X-48.2 F3000
G1 X-28.5 F12000
G1 X-48.2 F3000
M400
G1 Z{max_layer_z + 3.0} F2000
M106 P1 S0

{if layer_num < 0}
;校准挤出
G1 X108.000 Y0 F30000
G1 Z0.300 F1200
M400
G2814 Z0.32
M400
    M900 S
    M900 C
    G90
    M83
    M109 S{nozzle_temperature_initial_layer[initial_extruder]}
    G0 X128 E8  F{outer_wall_volumetric_speed/(24/20)    * 60}
    G0 X133 E.3742  F{outer_wall_volumetric_speed/(0.3*0.5)/4     * 60}
    G0 X138 E.3742  F{outer_wall_volumetric_speed/(0.3*0.5)     * 60}
    G0 X143 E.3742  F{outer_wall_volumetric_speed/(0.3*0.5)/4     * 60}
    G0 X148 E.3742  F{outer_wall_volumetric_speed/(0.3*0.5)     * 60}
    G0 X153 E.3742  F{outer_wall_volumetric_speed/(0.3*0.5)/4     * 60}
    G91
    G1 X1 Z-0.1500
    G1 X4
    G1 Z1 F1200
    G90
    M83
    M400
{endif}

{if layer_z <= (initial_layer_print_height + 0.001)}
G1 X[x_after_toolchange]  Y[y_after_toolchange] F12000
G1 Z{z_after_toolchange+1} F1200
{endif}

M620 S[next_extruder]A
T[next_extruder]
M621 S[next_extruder]A
G392 S0
M1007 S1
```
## P1系列
在耗材丝更换Gcode的最上面，插入这一段：
```
{if (toolchange_count)== 1}
M73 P{110+next_extruder} R[next_extruder] L5
{else}
M73 P{110+next_extruder} R[next_extruder] L{10+previous_extruder}
{endif}
M400 U1
```
## X1系列
** 还未适配，联系我我们一起适配！你可以先尝试使用P1系列的Gcode ** 
