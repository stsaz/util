
struct vlan_hdr {
	u_char info[2]; // PCP[3]  DEI[1]  VID[12]
	u_char eth_type[2]; // enum ETH_T
};

#define vlan_hdr_id(vh)  (ffint_be_cpu16_ptr((vh)->info) & 0x0fff)
