## How to make a DirectServo:

---
#### 1) Order:
A. Choose a pcb manufacturing or assembly house (such as PCBway), and select the details listed in: ***DirectServo-PCBway_Specifications.txt***
B. Supply the gerber files in ***DirectServo-Gerber.zip***
C. Optionally supply the following files if you're having them assemble the boards, otherwise manually purchase and assemble the following:
>* ***DirectServo-BOM.csv***   - Bill of materials for pcb assembly
>* ***DirectServo-all-pos.csv***  - Positions for BOM components used by pcb assembly service
>* ***DirectServo-Cathodes_And_Pin1s.png***  - Clarification of pin 1 locations for polarized components

---
#### 3) Manual assembly:
After receiving the boards, solder on the manually assembled parts (according to your application's usages). These additional parts are listed in: **DirectServo-BOM-Manual_Assembly_Parts.csv**

---
#### 4) Programing.
It's suggested a ST-LINK-V2-ISOL is used, along with a ARM-JTAG-20-10 Micro JTAG adapter (make sure to install the usb drivers).
* https://www.digikey.com/product-detail/en/stmicroelectronics/ST-LINK-V2-ISOL/497-15961-ND/4357213
* https://www.amazon.com/PACK-ARM-JTAG-20-10-Micro-JTAG-adapter/dp/B010ATK9OC

You can use your preferred development environment to compile the code-base.
>**For eclipse:**
>1. Install eclipse for C/C++
>2. Install the "System Workbench for STM32" plugin using eclipse's plugin manager with the setting: "System Workbench for STM32 - Bare Machine edition" - http://ac6-tools.com/Eclipse-updates/org.openstm32.system-workbench.update-site-v2
>3. Create a "Ac6 STM32 MCU GCC" project.
>4. Select the STM32F031C6Tx MCU
>5. Select the bare bones "Cube HAL" Hardware Abstraction Layer
>6. Copy the /Firmware source files into the newly created source folder and build/program

---
#### 5) Testing:
You can test the completed board(s) using a usb to RS485 adapter such as a "JBtek USB to RS485 Converter Adapter" and any uart serial communication software such as putty or python.
If you're using python, walk through the /Testing/examples.py file to get started.
