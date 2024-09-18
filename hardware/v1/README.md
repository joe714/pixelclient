# Summary
V1 Hub75 driver board.

Order from OSH Park: https://oshpark.com/shared_projects/DTnHhcQ1

**The USB port on this board is for power only** for cost reasons. You will need a USB to TTL serial
adapter connected to J2 to do the initial firmware load.

J3 is the data connector for the HUB 75 panel. There isn't enough clearance to use a keyed shrouded header,
so make sure you get the ribbon cable orientation correct (key should face towards the center of the board).

Frame-P4 and Backplate-P4.stl match up with P4 HUB75 displays like 
this one: https://www.aliexpress.us/item/2251832328529483.html?spm=a2g0o.order_list.order_list_main.11.b1cd1802htAgJk&gatewayAdapt=glo2usa.
Use 3 M3x3 (OD 4.2) heat set inserts and M3x5 screws to fasten the PCB to the backplate,
and 6 M3x40 bolts to fasten the backplate to the panel, sandwiching the frame.

Note this is a larger display than the original Tidbyt hardware. If you want to
use a different pitch display, you'll need to work out your own mounting.

# BOM
|Item |Qty  |Reference(s)  |Value   |LibPart|Footprint|Comments     |
|-----|-----|--------------|--------|-------|---------|-------------|
|1|1|C1|1uF|Device:C|Capacitor\_SMD:C\_0805\_2012Metric\_Pad1.18x1.45mm\_HandSolder||
|2|2|C2, C3|10uF|Device:C|Capacitor\_SMD:C\_0805\_2012Metric\_Pad1.18x1.45mm\_HandSolder||
|3|1|C4|4.7uF|Device:C|Capacitor\_SMD:C\_0805\_2012Metric\_Pad1.18x1.45mm\_HandSolder||
|4|1|C5|.1uF|Device:C|Capacitor\_SMD:C\_0805\_2012Metric\_Pad1.18x1.45mm\_HandSolder||
|5|1|D1|LED|Device:LED|LED\_SMD:LED\_0603\_1608Metric|DNP|
|7|1|J1|USB\_C\_Receptacle\_USB2.0|Connector:USB\_C\_Receptacle\_USB2.0|Connector\_TH\_Local:USB\_C\_2.0\_Vertical|https://www.aliexpress.us/item/3256803989627803.html?spm=a2g0o.order_list.order_list_main.10.35971802Oiyejk&gatewayAdapt=glo2usa|
|8|1|J2|ISP|Connector\_Generic:Conn\_02x03\_Odd\_Even|Connector\_PinHeader\_2.54mm:PinHeader\_2x03\_P2.54mm\_Vertical|ISP header|
|9|1|J3|HUB75|Connector\_Generic:Conn\_02x08\_Odd\_Even|Connector\_PinSocket\_2.54mm:PinSocket\_2x08\_P2.54mm\_Vertical|Hub75 data connector|
|10|1|J4|Screw\_Terminal\_01x02|Connector:Screw\_Terminal\_01x02|TerminalBlock\_TE-Connectivity:TerminalBlock\_TE\_282834-2\_1x02\_P2.54mm\_Horizontal|Hub75 power connector|
|11|1|J5|Conn\_01x03\_Male|Connector:Conn\_01x03\_Male|Connector\_JST:JST\_PH\_B3B-PH-K\_1x03\_P2.00mm\_Vertical|DNP, for future use|
|12|1|R1|10K|Device:R|Resistor\_SMD:R\_0603\_1608Metric||
|13|2|R2, R3|5.1K|Device:R|Resistor\_SMD:R\_0603\_1608Metric||
|14|1|R4|750|Device:R|Resistor\_SMD:R\_0603\_1608Metric|DNP|
|15|2|R6, R7|DNP|Device:R|Resistor\_SMD:R\_0603\_1608Metric||
|16|1|SW1|PRG|Switch:SW\_Push|Button\_Switch\_SMD\_Local:ESWITCH\_TL3302G|DNP|
|17|1|SW2|RST|Switch:SW\_Push|Button\_Switch\_SMD\_Local:ESWITCH\_TL3302G|DNP|
|18|1|U1|ESP32-WROOM-32D|RF\_Module:ESP32-WROOM-32D|RF\_Module:ESP32-WROOM-32|https://www.espressif.com/sites/default/files/documentation/esp32-wroom-32d_esp32-wroom-32u_datasheet_en.pdf, use D or later revision and 8MB flash|
|19|1|U2|TLV1117-33|Regulator\_Linear:TLV1117-33|Package\_TO\_SOT\_SMD:SOT-223-3\_TabPin2|http://www.ti.com/lit/ds/symlink/tlv1117.pdf|

