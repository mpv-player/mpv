
// methods from class VbrControl

	void VbrControl_init_1pass_vbr(int quality, int crispness);
	int VbrControl_init_2pass_vbr_encoding(const char* filename, int bitrate, double framerate, int crispness, int quality);
	int VbrControl_init_2pass_vbr_analysis(const char* filename, int quality);

	void VbrControl_update_1pass_vbr();
	void VbrControl_update_2pass_vbr_encoding(int motion_bits, int texture_bits, int total_bits);
	void VbrControl_update_2pass_vbr_analysis(int is_key_frame, int motion_bits, int texture_bits, int total_bits, int quant);

	int VbrControl_get_quant();
	void VbrControl_set_quant(float q);
	int VbrControl_get_intra();
	short VbrControl_get_drop();
	void VbrControl_close();

