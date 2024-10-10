# 5G gNB Frequency Configuration

Configuring the frequency settings for a 5G gNB involves specifying multiple parameters in the gNB configuration file. This guide outlines the necessary parameters and provides step-by-step instructions for configuration, focusing on FR1 TDD bands above 3000 MHz.

## Configuration parameters

To configure the gNB with the required frequency and bandwidth, modify the following parameters in the configuration file:

### `gNBs` Section

* `absoluteFrequencySSB`: ARFCN (frequency number) of the middle frequency of the SSB. Steps to compute this value are provided below.
* `dl_absoluteFrequencyPointA`: ARFCN (frequency number) for Point A, lowest subcarrier of the reference resource block (Common RB 0). Steps to compute this value are provided below.
* `dl_frequencyBand`: downlink NR frequency band number.
* `dl_subcarrierSpacing`: downlink subcarrier spacing.
  * *Allowed values:* `0` = 15 kHz, `1` = 30 kHz, `2` = 60 kHz, `3` = 120 kHz
* `dl_carrierBandwidth`: number of PRBs for downlink.
* `initialDLBWPlocationAndBandwidth`: resource indicator value (RIV) of the initial BWP. Steps to compute this value are provided below.
* `initialDLBWPsubcarrierSpacing`: initial downlink BWP subcarrier spacing.
  * *Allowed values:* `0` = 15 kHz, `1` = 30 kHz, `2` = 60 kHz, `3` = 120 kHz
* `ul_frequencyBand`: uplink NR frequency band number.
* `ul_carrierBandwidth`: number of PRBs for uplink.
* `initialULBWPlocationAndBandwidth`: resource indicator value (RIV) of the initial BWP. Steps to compute this value are provided below.
* `initialULBWPsubcarrierSpacing`: initial uplink BWP subcarrier spacing.
  * *Allowed values:* `0` = 15 kHz, `1` = 30 kHz, `2` = 60 kHz, `3` = 120 kHz
* `subcarrierSpacing`: general subcarrier spacing for both DL and UL.
  * *Allowed values:* `0` = 15 kHz, `1` = 30 kHz, `2` = 60 kHz, `3` = 120 kHz

### `RUs` Section

* `bands`: NR frequency band number for RU(s).

## Determining the ARFCN value for `absoluteFrequencySSB`

Follow these steps to compute the ARFCN value for `absoluteFrequencySSB` within the global 5G NR synchronization raster:

1. Identify the SSB frequency in the synchronization raster
* Use formula given in TS 38.104, Section 5.4.3.1 Synchronization Raster:
2. Convert the calculated frequency to ARFCN
* Use an online ARFCN calculator to convert the frequency (in MHz) to the corresponding ARFCN value.
  * Example tool: [5G NR ARFCN Calculator](https://5g-tools.com/5g-nr-arfcn-calculator/)
3. Set `absoluteFrequencySSB` in the gNB configuration file

Example calculation for center frequency of 3500 MHz:
* Note: This calculation is valid only for frequencies within the range of 3000 to 24250 MHz, as specified in TS 38.104, Section 5.4.3.1.
* Identify the closest SSB frequency in the synchronization raster
     $$
     \frac{\text{Center Frequency} - 3000}{1.44}
     $$

     $$
     \frac{3500 - 3000}{1.44} \approx 347.22
     $$
* Round the result to the nearest whole number
  * Rounded result: 347
* Calculate the SSB frequency using the rounded value (based on TS 38.104, Section 5.4.3.1)
     $$
     3000 + (347 \times 1.44) = 3499.68 \text{ MHz}
     $$
* Convert to ARFCN:
  * Frequency: 3499.68 MHz
  * Corresponding ARFCN: 633312


## Determining the ARFCN value for `dl_absoluteFrequencyPointA` and BWP-related parameters

Follow these steps to find the ARFCN values for `dl_absoluteFrequencyPointA`, `initialDLBWPlocationAndBandwidth`, and `initialULBWPlocationAndBandwidth`:

1. Enter parameters into the tool for NR Point A computation
* Example tool: [NR Reference Point A Tool](https://www.sqimway.com/nr_refA.php)
* Enter the frequency band, `absoluteFrequencySSB` (NR Arfcn scan parameter), subcarrier spacing (SCS parameter), and bandwidth into the tool.

2. Obtain the required values
* Example conguration for following input parameters
  * Frequency band = n78
  * NR Arfcn scan (`absoluteFrequencySSB`) = 633312
  * SCS = 30 kHz
  * Bandwidth = 100 MHz
* The tool provides the following values:
  * Point A Arfcnc (`dl_absoluteFrequencyPointA`) = 630036
  * locationAndBandwidth full (`initialDLBWPlocationAndBandwidth`) = 1099
  * locationAndBandwidth full (`initialULBWPlocationAndBandwidth`) = 1099

3. Set `dl_absoluteFrequencyPointA`, `initialDLBWPlocationAndBandwidth`, and `initialULBWPlocationAndBandwidth` in the gNB configuration file
