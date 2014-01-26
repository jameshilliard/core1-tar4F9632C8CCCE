#ifndef AMAZON_ATM_MIB_H
#define AMAZON_ATM_MIB_H
typedef struct{
	__u32	ifHCInOctets_h;
	__u32	ifHCInOctets_l;
	__u32	ifHCOutOctets_h;
	__u32	ifHCOutOctets_l;
	__u32	ifInErrors;
	__u32	ifInUnknownProtos;
	__u32	ifOutErrors;
}atm_cell_ifEntry_t;

typedef struct{
	__u32	ifHCInOctets_h;
	__u32	ifHCInOctets_l;
	__u32	ifHCOutOctets_h;
	__u32	ifHCOutOctets_l;
	__u32	ifInUcastPkts;
	__u32	ifOutUcastPkts;
	__u32	ifInErrors;
	__u32	ifInDiscards;
	__u32	ifOutErros;
	__u32	ifOutDiscards;
}atm_aal5_ifEntry_t;

typedef struct{
	__u32	aal5VccCrcErrors;
	__u32	aal5VccSarTimeOuts;//no timer support yet
	__u32	aal5VccOverSizedSDUs;
}atm_aal5_vcc_t;
#endif //AMAZON_ATM_MIB_H
