# Firmware end user instructions

![](https://raw.githubusercontent.com/zebarnabe/DIGIduino/refs/heads/astro/Media/TPW_astro_custom_firmware.png)

The above image ilustrates the implemented firmware modes and views.
The interface is very similar to the one present in Casio watches.

The arrows are color coded to match the buttons, pointing to where you go when pressing that button.

In practice:
The top left button (red) is the **Set**/Reset/Split/View button.
The bottom left button (green) is the **Mode**/Select button.
The top right button (blue) is the **Up**/Detail/Wake button.
The bottom right button (yellow) is the **Down**/Start/Stop button.

Demo: [wokwi](https://wokwi.com/projects/440281421500083201)

The **Up** will wake the watch if it goes to sleep.

## Main modes
The **Mode** button will change the mode:
* Time → Displays time
* Chrono → Stop watch 
* Moon → Lunar data
* Sun → Solar data

Holding the **Mode** button will cancel the current action and go back to display time.

## Time mode ⌚
Pressing **Set** will change the view:
* Hour → Hour and minutes
* Date → Day and Month (order can be changed)
* Weekday → Abbreviature of the weekday name (Sun, Mon, Tue, Wed, Thu, Fri, Sat)
* Year → The current year

Pressing **Mode** button will go back to Hour view.

Holding **Set** button will enter Time setting mode.

## Time Setting mode 🛠
When entering this mode the display will briefly display `SEt `.

Pressing **Set** button will save changes and go back to Time mode.

Pressing **Up** or **Down** buttons will change the values for the current field.

Pressing **Mode** will change the setting view:
* Hour → Display will show hours and minutes where hours will be blinking
* Minute → Display will show hours and minutes where minutes will be blinking
* Day → Display will show `dY` followed by the day of the month blinking
* Month → Display will show `Mo` followed by the month blinking
* Year → Display will show the year blinking
* Format → Changes between day/month (`dYMo`) and month/day (`ModY`) in the dates displayed
* Display Time → Display will show `SLP` followed by the number of seconds that the screen stays on before going to sleep, note that this change is applied immediately (cancelling by holding **Mode** will not revert this). The minimum value is 3 and the maximum is 9.
* Brightness → Changes brightness between 25 and 100. (N.B.: This is not working)

## Chrono mode ⏱
When entering this mode the display will briefly display `Chrn`.

Pressing **Up** will show the minutes count for the current state.

Chrono mode has 3 states:
* Stopped → Initial state, time measurement is stopped.<br>**Set** button will reset to zero.<br>**Down** button will change to Running.
* Running → Time is being counted.<br>**Set** button will go into Split state.<br>**Down** button will change to Stopped.
* Split → Shows the time when entered the split. Time is still being counted in the background. Display will briefly blink `SPLt`.<br>**Set** button will go back to Running state.<br>**Down** button will change to Stopped.

## Moon mode 🌙
When entering this mode the display will briefly display `Moon`. It will keep blinking `Moon` while in the Phase view.

Pressing **Up** or **Down** will change the view mode:
* Phase → Will show the current state of the moon (sorry south hemisphere):
   * `....` → 🌑 New Moon
   * `   )` → 🌒 Waxing Crescent
   * `  ()` → 🌓 First Quarter
   * ` (░)` → 🌔 Waxing Gibbous
   * `(░░)` → 🌕 Full Moon
   * `(░) ` → 🌖 Waning Gibbous
   * `()  ` → 🌗 Last Quarter
   * `(   ` → 🌘 Waning Crescent
* Full Moon → Date of the next full moon, will blink `Full`.
* New Moon → Date of the next new moon, will blink `NEW `.

## Sun mode 🌞
When entering this mode the display will briefly display `Sun `.

Holding **Set** button will change to Local setting mode.

Pressing **Up** or **Down** will change the view mode:
* Noon → Will display the time of solar noon (highest point) for the current day. Will blink `noon`.
* Sunrise → Will display the time of sun rise for the current day. Will blink `riSE`.
* Sunset → Will display the time of sun set for the current day. Will blink `SSEt`.

## Local Setting mode 🌐

Pressing **Set** button will save changes and go back to Sun mode.

Pressing **Up** or **Down** buttons will change the values for the current field.

Pressing **Mode** will change the setting view:
* Timezone offset hour → Defines the hour UTC offset for the current time, for example: Pacific Daylight Time, Los Angeles (GMT-7) should have this values defined at -7. The displayed value is followed by a `h`, the value will be blinking.
* Timezone offset minutes → Defined the minute UTC offset for the current time. The value will be blinking.
* Latitude Degrees → The degrees of latitude of the user location, the value will be blinking be followed by a `S` (for negative latitudes) or a `N`.
* Latitude Minutes → The minutes component of the latitude of the user location, the value will be blinking be followed by a `'`.
* Latitude Seconds → The seconds component of the latitude of the user location, the value will be blinking be followed by a `"`.
* Longitude Degrees → The degrees of Longitude of the user location, the value will be blinking be followed by a `W` (for negative longitudes) or a `E`.
* Longitude Minutes → The minutes component of the Longitude of the user location, the value will be blinking be followed by a `'`.
* Longitude Seconds → The seconds component of the Longitude of the user location, the value will be blinking be followed by a `"`.
