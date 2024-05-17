<table style="border-collapse: collapse; border: none;">
  <tr style="border-collapse: collapse; border: none;">
    <td style="border-collapse: collapse; border: none;">
      <a href="http://www.openairinterface.org/">
         <img src="./images/oai_final_logo.png" alt="" border=3 height=50 width=150>
         </img>
      </a>
    </td>
    <td style="border-collapse: collapse; border: none; vertical-align: center;">
      <b><font size = "5">OAI 5G NR SA tutorial with OAI nrUE</font></b>
    </td>
  </tr>
</table>

**Table of Contents**

[[_TOC_]]

#  1. Scenario
In this tutorial we describe how to configure and run a 5G end-to-end setup with OAI CN5G, OAI gNB and OAI nrUE.

Minimum hardware requirements:
- Laptop/Desktop/Server for OAI CN5G and OAI gNB
    - Operating System: [Ubuntu 22.04 LTS](https://releases.ubuntu.com/22.04/ubuntu-22.04.4-desktop-amd64.iso)
    - CPU: 8 cores x86_64 @ 3.5 GHz
    - RAM: 32 GB
- Laptop for UE
    - Operating System: [Ubuntu 22.04 LTS](https://releases.ubuntu.com/22.04/ubuntu-22.04.4-desktop-amd64.iso)
    - CPU: 8 cores x86_64 @ 3.5 GHz
    - RAM: 8 GB
- [USRP B210](https://www.ettus.com/all-products/ub210-kit/), [USRP N300](https://www.ettus.com/all-products/USRP-N300/) or [USRP X300](https://www.ettus.com/all-products/x300-kit/)
    - Please identify the network interface(s) on which the USRP is connected and update the gNB configuration file

# 2. OAI CN5G

## 2.1 OAI CN5G pre-requisites

Please install and configure OAI CN5G as described here:
[OAI CN5G](NR_SA_Tutorial_OAI_CN5G.md)

# 3. OAI gNB and OAI nrUE

## 3.1 OAI gNB and OAI nrUE pre-requisites

### Build UHD from source
```bash
# https://files.ettus.com/manual/page_build_guide.html
sudo apt install -y autoconf automake build-essential ccache cmake cpufrequtils doxygen ethtool g++ git inetutils-tools libboost-all-dev libncurses5 libncurses5-dev libusb-1.0-0 libusb-1.0-0-dev libusb-dev python3-dev python3-mako python3-numpy python3-requests python3-scipy python3-setuptools python3-ruamel.yaml

git clone https://github.com/EttusResearch/uhd.git ~/uhd
cd ~/uhd
git checkout v4.6.0.0
cd host
mkdir build
cd build
cmake ../
make -j $(nproc)
make test # This step is optional
sudo make install
sudo ldconfig
sudo uhd_images_downloader
```

## 3.2 Build OAI gNB and OAI nrUE

```bash
# Get openairinterface5g source code
git clone https://gitlab.eurecom.fr/oai/openairinterface5g.git ~/openairinterface5g
cd ~/openairinterface5g
git checkout develop

# Install OAI dependencies
cd ~/openairinterface5g/cmake_targets
./build_oai -I

# nrscope dependencies
sudo apt install -y libforms-dev libforms-bin

# Build OAI gNB
cd ~/openairinterface5g/cmake_targets
./build_oai -w USRP --ninja --nrUE --gNB --build-lib "nrscope" -C
```

# 4. Run OAI CN5G and OAI gNB

## 4.1 Run OAI CN5G

```bash
cd ~/oai-cn5g
docker compose up -d
```

## 4.2 Run OAI gNB

### USRP B210
```bash
cd ~/openairinterface5g/cmake_targets/ran_build/build
sudo ./nr-softmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.sa.band78.fr1.106PRB.usrpb210.conf --gNBs.[0].min_rxtxtime 6 --sa -E --continuous-tx
```
### USRP N300
```bash
cd ~/openairinterface5g/cmake_targets/ran_build/build
sudo ./nr-softmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.sa.band77.fr1.273PRB.2x2.usrpn300.conf --gNBs.[0].min_rxtxtime 6 --sa --usrp-tx-thread-config 1
```

### USRP X300
```bash
cd ~/openairinterface5g/cmake_targets/ran_build/build
sudo ./nr-softmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.sa.band77.fr1.273PRB.2x2.usrpn300.conf --gNBs.[0].min_rxtxtime 6 --sa --usrp-tx-thread-config 1 -E --continuous-tx
```

# 5. OAI UE

## 5.1 Run OAI nrUE
### USRP B210
Important notes:
- This should be run in a second Ubuntu 22.04 host, other than gNB
- It only applies when running OAI gNB with USRP B210

Run OAI nrUE with USRP B210
```bash
cd ~/openairinterface5g/cmake_targets/ran_build/build
sudo ./nr-uesoftmodem -r 106 --numerology 1 --band 78 -C 3619200000 --ue-fo-compensation --sa -E --uicc0.imsi 001010000000001
```
## 5.2 End-to-end connectivity test

Ping test from the UE host to the CN5G:

```bash
ping 192.168.70.135 -I oaitun_ue1
```

# 6. Run an end-to-end OAI setup with RFsimulator

Please refer to the following documentation to learn about the relevant topics discussed in this chapter:

- RFsimulator tutorial [rfsimulator/README.md](../radio/rfsimulator/README.md)
- Channel simulation with OAI [channel_simulation.md](../openair1/SIMULATION/TOOLS/DOC/channel_simulation.md)
- Telnet server usage [telnetusage.md](../common/utils/telnetsrv/DOC/telnetusage.md).

## 6.1 Build

In case of deployment with RFsimulator, build with:

```bash
# Build OAI gNB
cd ~/openairinterface5g/cmake_targets
./build_oai -w SIMU --ninja --nrUE --gNB --build-lib "nrscope" -C
```
## 6.2 Run gNB

### 6.2.1 FR1
```bash
cd ~/openairinterface5g/cmake_targets/ran_build/build
sudo ./nr-softmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.sa.band78.fr1.106PRB.usrpb210.conf --gNBs.[0].min_rxtxtime 6 --rfsim --sa
```

### 6.2.2 FR2
```bash
cd ~/openairinterface5g/cmake_targets/ran_build/build
sudo ./nr-softmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.sa.band257.u3.32prb.usrpx410.conf --rfsim
```

## 6.3 Run nrUE

### 6.3.1 FR1

Important notes:
- This should be run on the same host as the OAI gNB
- It only applies when running OAI gNB with RFsimulator

Run OAI nrUE with RFsimulator
```bash
cd ~/openairinterface5g/cmake_targets/ran_build/build
sudo ./nr-uesoftmodem -r 106 --numerology 1 --band 78 -C 3619200000 --sa --uicc0.imsi 001010000000001 --rfsim
```

### 6.3.2 FR2
Important notes:
- This should be run on the same host as the OAI gNB
- It only applies when running OAI gNB with RFsimulator in FR2

Run OAI nrUE with RFsimulator in FR2
```bash
cd ~/openairinterface5g/cmake_targets/ran_build/build
sudo ./nr-uesoftmodem -r 32 --numerology 3 --band 257 -C 27533280000 --sa --uicc0.imsi 001010000000001 --ssb 72 --rfsim
```

## 6.3.4 Run multiple UEs in RFsimulator

### 6.3.5 Multiple nrUEs with namespaces

Important notes:

* This should be run on the same host as the OAI gNB
* It only applies when running OAI gNB with RFsimulator
* Use the script [multi_ue.sh](../radio/rfsimulator/scripts/multi-ue.sh) to make namespaces for multiple UEs.
* For each UE, a namespace shall be created, each one has a different address that will be used as rfsim server address
* Each UE shall have a different IMSI, which shall be present in the relevant tables of the MySQL database
* Each UE shall run a telnet server on a different port, with command line option `--telnetsrv.listenport` 

1. For the first UE, create the namespace ue1 (-c1) and then execute bash inside (-e):

```bash
sudo ./multi-ue.sh -c1 -e
sudo ./multi-ue.sh -o1
```

2. After entering the bash environment, run the following command to deploy your first UE

```bash
sudo ./nr-uesoftmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/ue.conf -r 106 --numerology 1 --band 78 -C 3619200000 --rfsim --sa --uicc0.imsi 001010000000001 --nokrnmod -E --rfsimulator.options chanmod --rfsimulator.serveraddr 10.201.1.100 --telnetsrv --telnetsrv.listenport 9095
```

3. For the second UE, create the namespace ue2 (-c2) and then execute bash inside (-e):

```bash
sudo ./multi-ue.sh -c2 -e
sudo ./multi-ue.sh -o2
```

4. After entering the bash environment, run the following command to deploy your second UE
```bash
sudo ./nr-uesoftmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/ue.conf -r 106 --numerology 1 --band 78 -C 3619200000 --rfsim --sa --uicc0.imsi 001010000000002 --nokrnmod -E --rfsimulator.options chanmod --rfsimulator.serveraddr 10.202.1.100 --telnetsrv --telnetsrv.listenport 9096
```

### 6.3.6 Running Multiple UEs with Docker

1. Make sure OAI nrUE image is pulled:

```bash
docker pull oaisoftwarealliance/oai-nr-ue:latest
```

2. Configure your setup by editing the Docker compose file e.g. in [docker-compose.yaml](../ci-scripts/yaml_files/5g_rfsimulator/docker-compose.yaml). 

3. Deploy the UEs, e.g. for 3 UEs:

```bash
docker compose up -d oai-nr-ue{1,2,3}
```

4. Check the logs to ensure each UE has gotten an IP address, e.g.:

```bash
docker logs oai-nr-ue1
```

or

```bash
  docker exec -it oai-nr-ue1 ip a show oaitun_ue1
```

5. Test the connectivity of each UE to the core network:
```bash
  docker exec -it oai-nr-ue1 ping -c1 192.168.70.135
```

7. After testing, undeploy the UEs to allow them time to deregister, and then bring down the rest of the network:
```bash
  docker compose stop oai-nr-ue{1,2,3}
  docker compose down -v
```

For more details and scenario, refer to the following files:

* [RFSIM deployment in the CI](../ci-scripts/yaml_files/5g_rfsimulator/README.md)
* [E1 deployment in the CI](../ci-scripts/yaml_files/5g_rfsimulator_e1/README.md)
* [Docker documentation](../docker/README.md)

# 7. Connect OAI nrUE to an NG-Core

A configuration file can be fed to the nrUE command line in order to connect to the local NGC.

The nrUE configuration file (e.g. [ue.conf](../targets/PROJECTS/GENERIC-NR-5GC/CONF/ue.conf)) or [ue.sa.conf](../ci-scripts/conf_files/ue.sa.conf) is structured in a key-value format and contains the relevant UICC parameters that are necessary to authenticate the UE to the local 5GC. E.g.:

```shell
uicc0 = {
  imsi = "001010000000001";
  key = "fec86ba6eb707ed08905757b1bb44b8f";
  opc = "C42449363BBAD02B66D16BC975D77CC1";
  dnn = "oai";
  nssai_sst = 1;
}
```

| **Parameter** | **Description** | **Default Value** |
|---------------|-----------------|-------------------|
| **IMSI** | Unique identifier for the UE within the mobile network. Used by the network to identify the UE during authentication. It ensures that the UE is correctly identified by the network. | 001010000000001 |
| **key** | Cryptographic key shared between the UE and the network, used for encryption during the authentication process. | `fec86ba6eb707ed08905757b1bb44b8f` |
| **OPC** | Operator key for the Milenage Authentication and Key Agreement algorithm used for encryption during the authentication process. | Ensures secure communication between the UE and the network by matching the encryption keys. | `C42449363BBAD02B66D16BC975D77CC1` |
| **DNN** | Specifies the name of the data network the UE wishes to connect to, similar to an APN in 4G networks. | `oai` |
| **NSSAI** | Allows the UE to select the appropriate network slice, which provides different QoS. | `1` |

The UE configuration must match the one of the network's AMF. The nrUE can connect by default to OAI CN5G with no need to provide the configuration file.

When running the `nr-uesoftmodem`, one can specify the nrUE configuration file using the `-O` option. E.g.:

```bash
sudo ./nr-uesoftmodem --rfsim --rfsimulator.serveraddr 127.0.0.1 --sa -r 106 --numerology 1 --band 78 -C 3619200000 -O ~/nrue.uicc.conf
```
The CL option `--uicc0.imsi`  can override the IMSI value in the configuration file if necessary (e.g. when running multiple UEs): `--uicc0.imsi  001010000000001`.

More details available at [ci-scripts/yaml_files/5g_rfsimulator/README.md](../ci-scripts/yaml_files/5g_rfsimulator/README.md).

# 7. Advanced configurations (optional)

## 7.1 USRP N300 and X300 Ethernet Tuning

Please also refer to the official [USRP Host Performance Tuning Tips and Tricks](https://kb.ettus.com/USRP_Host_Performance_Tuning_Tips_and_Tricks) tuning guide.

The following steps are recommended. Please change the network interface(s) as required. Also, you should have 10Gbps interface(s): if you use two cables, you should have the XG firmware. Refer to the [N300 Getting Started Guide](https://kb.ettus.com/USRP_N300/N310/N320/N321_Getting_Started_Guide) for more information.

* Use an MTU of 9000: how to change this depends on the network management tool. In the case of Network Manager, this can be done from the GUI.
* Increase the kernel socket buffer (also done by the USRP driver in OAI)
* Increase Ethernet Ring Buffers: `sudo ethtool -G <ifname> rx 4096 tx 4096`
* Disable hyper-threading in the BIOS (This step is optional)
* Optional: Disable KPTI Protections for Spectre/Meltdown for more performance. **This is a security risk.** Add `mitigations=off nosmt` in your grub config and update grub. (This step is optional)

Example code to run:
```
for ((i=0;i<$(nproc);i++)); do sudo cpufreq-set -c $i -r -g performance; done
sudo sysctl -w net.core.wmem_max=62500000
sudo sysctl -w net.core.rmem_max=62500000
sudo sysctl -w net.core.wmem_default=62500000
sudo sysctl -w net.core.rmem_default=62500000
sudo ethtool -G enp1s0f0 tx 4096 rx 4096
```

## 7.2 Real-time performance workarounds
- Enable Performance Mode `sudo cpupower idle-set -D 0`
- If you get real-time problems on heavy UL traffic, reduce the maximum UL MCS using an additional command-line switch: `--MACRLCs.[0].ul_max_mcs 14`.
- You can also reduce the number of LDPC decoder iterations, which will make the LDPC decoder take less time: `--L1s.[0].max_ldpc_iterations 4`.

## 7.3 Uplink issues related with noise on the DC carriers
- There is noise on the DC carriers on N300 and especially the X300 in UL. To avoid their use or shift them away from the center to use more UL spectrum, use the `--tune-offset <Hz>` command line switch, where `<Hz>` is ideally half the bandwidth, or possibly less.

## 7.4 Timing-related Problems
- Sometimes, the nrUE would keep repeating RA procedure because of Msg3 failure at the gNB. If it happens, add the `-A` option at the nrUE and/or gNB side, e.g., `-A 45`. This modifies the timing advance (in samples). Adjust +/-5 if the issue persists.
- This can be necessary since certain USRPs have larger signal delays than others; it is therefore specific to the used USRP model.
- The x310 and B210 are found to work with the default configuration; N310 and x410 can benefit from setting this timing advance.
- For example if the OAI UE uses the X410 and the gNB based on [Nvidia Aerial and Foxconn](./Aerial_FAPI_Split_Tutorial.md) a timing advance of 90 has been found to work well.  

## 7.5 Lower latency on user plane
- To lower latency on the user plane, you can force the UE to be scheduled constantly in uplink: `--MACRLCs.[0].ulsch_max_frame_inactivity 0` .
