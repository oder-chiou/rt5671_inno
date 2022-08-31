/*
 * rt5671.c  --  RT5671 ALSA SoC DSP driver
 *
 * Copyright 2011 Realtek Semiconductor Corp.
 * Author: Johnny Hsu <johnnyhsu@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/firmware.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include "rt5671.h"
#include "rt5671-dsp.h"

#define DSP_CLK_RATE RT5671_DSP_CLK_96K

/* DSP mode */
static unsigned short rt5671_dsp_ns[][2] = {
	{0x22f8, 0x8005}, {0x2375, 0x7ff0}, {0x2376, 0x7990}, {0x2377, 0x7332},
	{0x2388, 0x7fff}, {0x2389, 0x6000}, {0x238a, 0x0000}, {0x238b, 0x1000},
	{0x238c, 0x1000}, {0x23a1, 0x2000}, {0x2303, 0x0200}, {0x2304, 0x0032},
	{0x2305, 0x0000}, {0x230c, 0x0200}, {0x22fb, 0x0000}
};
#define RT5671_DSP_NS_NUM \
	(sizeof(rt5671_dsp_ns) / sizeof(rt5671_dsp_ns[0]))

/**
 * rt5671_dsp_done - Wait until DSP is ready.
 * @codec: SoC Audio Codec device.
 *
 * To check voice DSP status and confirm it's ready for next work.
 *
 * Returns 0 for success or negative error code.
 */
static int rt5671_dsp_done(struct snd_soc_codec *codec)
{
	unsigned int count = 0, dsp_val;

	dsp_val = snd_soc_read(codec, RT5671_DSP_CTRL1);
	while (dsp_val & RT5671_DSP_BUSY_MASK) {
		if (count > 10)
			return -EBUSY;
		dsp_val = snd_soc_read(codec, RT5671_DSP_CTRL1);
		count++;
	}

	return 0;
}

/**
 * rt5671_dsp_write - Write DSP register.
 * @codec: SoC audio codec device.
 * @param: DSP parameters.
  *
 * Modify voice DSP register for sound effect. The DSP can be controlled
 * through DSP command format (0xfc), addr (0xc4), data (0xc5) and cmd (0xc6)
 * register. It has to wait until the DSP is ready.
 *
 * Returns 0 for success or negative error code.
 */
int rt5671_dsp_write(struct snd_soc_codec *codec,
		unsigned int addr, unsigned int data)
{
	unsigned int dsp_val;
	int ret;

	ret = snd_soc_write(codec, RT5671_DSP_CTRL2, addr);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to write DSP addr reg: %d\n", ret);
		goto err;
	}
	ret = snd_soc_write(codec, RT5671_DSP_CTRL3, data);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to write DSP data reg: %d\n", ret);
		goto err;
	}
	dsp_val = RT5671_DSP_I2C_AL_16 | RT5671_DSP_DL_2 |
		RT5671_DSP_CMD_MW | DSP_CLK_RATE | RT5671_DSP_CMD_EN;

	ret = snd_soc_write(codec, RT5671_DSP_CTRL1, dsp_val);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to write DSP cmd reg: %d\n", ret);
		goto err;
	}
	ret = rt5671_dsp_done(codec);
	if (ret < 0) {
		dev_err(codec->dev, "DSP is busy: %d\n", ret);
		goto err;
	}

	return 0;

err:
	return ret;
}

/**
 * rt5671_dsp_read - Read DSP register.
 * @codec: SoC audio codec device.
 * @reg: DSP register index.
 *
 * Read DSP setting value from voice DSP. The DSP can be controlled
 * through DSP addr (0xc4), data (0xc5) and cmd (0xc6) register. Each
 * command has to wait until the DSP is ready.
 *
 * Returns DSP register value or negative error code.
 */
unsigned int rt5671_dsp_read(
	struct snd_soc_codec *codec, unsigned int reg)
{
	unsigned int value;
	unsigned int dsp_val;
	int ret = 0;

	ret = rt5671_dsp_done(codec);
	if (ret < 0) {
		dev_err(codec->dev, "DSP is busy: %d\n", ret);
		goto err;
	}

	ret = snd_soc_write(codec, RT5671_DSP_CTRL2, reg);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to write DSP addr reg: %d\n", ret);
		goto err;
	}
	dsp_val = RT5671_DSP_I2C_AL_16 | RT5671_DSP_DL_0 | RT5671_DSP_RW_MASK |
		RT5671_DSP_CMD_MR | DSP_CLK_RATE | RT5671_DSP_CMD_EN;

	ret = snd_soc_write(codec, RT5671_DSP_CTRL1, dsp_val);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to write DSP cmd reg: %d\n", ret);
		goto err;
	}

	ret = rt5671_dsp_done(codec);
	if (ret < 0) {
		dev_err(codec->dev, "DSP is busy: %d\n", ret);
		goto err;
	}

	ret = snd_soc_write(codec, RT5671_DSP_CTRL2, 0x26);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to write DSP addr reg: %d\n", ret);
		goto err;
	}
	dsp_val = RT5671_DSP_DL_1 | RT5671_DSP_CMD_RR | RT5671_DSP_RW_MASK |
		DSP_CLK_RATE | RT5671_DSP_CMD_EN;

	ret = snd_soc_write(codec, RT5671_DSP_CTRL1, dsp_val);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to write DSP cmd reg: %d\n", ret);
		goto err;
	}

	ret = rt5671_dsp_done(codec);
	if (ret < 0) {
		dev_err(codec->dev, "DSP is busy: %d\n", ret);
		goto err;
	}

	ret = snd_soc_write(codec, RT5671_DSP_CTRL2, 0x25);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to write DSP addr reg: %d\n", ret);
		goto err;
	}

	dsp_val = RT5671_DSP_DL_1 | RT5671_DSP_CMD_RR | RT5671_DSP_RW_MASK |
		DSP_CLK_RATE | RT5671_DSP_CMD_EN;

	ret = snd_soc_write(codec, RT5671_DSP_CTRL1, dsp_val);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to write DSP cmd reg: %d\n", ret);
		goto err;
	}

	ret = rt5671_dsp_done(codec);
	if (ret < 0) {
		dev_err(codec->dev, "DSP is busy: %d\n", ret);
		goto err;
	}

	ret = snd_soc_read(codec, RT5671_DSP_CTRL5);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to read DSP data reg: %d\n", ret);
		goto err;
	}

	value = ret;
	return value;

err:
	return ret;
}

static int rt5671_dsp_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = rt5671->dsp_sw;

	return 0;
}

static int rt5671_dsp_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	rt5671->dsp_sw = ucontrol->value.integer.value[0];

	return 0;
}

/* DSP Path Control 1 */
static const char * const rt5671_src_rxdp_mode[] = {
	"Normal", "Divided by 2", "Divided by 3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_src_rxdp_enum, RT5671_DSP_PATH1,
	RT5671_RXDP_SRC_SFT, rt5671_src_rxdp_mode);

static const char * const rt5671_src_txdp_mode[] = {
	"Normal", "Multiplied by 2", "Multiplied by 3"
};

static const SOC_ENUM_SINGLE_DECL(
	rt5671_src_txdp_enum, RT5671_DSP_PATH1,
	RT5671_TXDP_SRC_SFT, rt5671_src_txdp_mode);

/* Sound Effect */
static const char * const rt5671_dsp_mode[] = {
	"Disable",
	"NS",
};

static const SOC_ENUM_SINGLE_DECL(rt5671_dsp_enum, 0, 0,
	rt5671_dsp_mode);

static const struct snd_kcontrol_new rt5671_dsp_snd_controls[] = {
	SOC_ENUM("RxDP SRC Switch", rt5671_src_rxdp_enum),
	SOC_ENUM("TxDP SRC Switch", rt5671_src_txdp_enum),
	/* AEC */
	SOC_ENUM_EXT("DSP Function Switch", rt5671_dsp_enum,
		rt5671_dsp_get, rt5671_dsp_put),
};

/**
 * rt5671_dsp_set_mode - Set DSP mode parameters.
 *
 * @codec: SoC audio codec device.
 * @mode: DSP mode.
 *
 * Set parameters of mode to DSP.
 * There are three modes which includes " mic AEC + NS + FENS",
 * "HFBF" and "Far-field pickup".
 *
 * Returns 0 for success or negative error code.
 */
static int rt5671_dsp_set_mode(struct snd_soc_codec *codec, int mode)
{
	int ret, i, tab_num;
	unsigned short (*mode_tab)[2];

	switch (mode) {
	case RT5671_DSP_NS:
		dev_info(codec->dev, "NS\n");
		mode_tab = rt5671_dsp_ns;
		tab_num = RT5671_DSP_NS_NUM;
		break;

	case RT5671_DSP_DIS:
	default:
		dev_info(codec->dev, "Disable\n");
		return 0;
	}

	for (i = 0; i < tab_num; i++) {
		ret = rt5671_dsp_write(codec, mode_tab[i][0], mode_tab[i][1]);
		if (ret < 0)
			goto mode_err;
	}

	return 0;

mode_err:

	dev_err(codec->dev, "Fail to set mode %d parameters: %d\n", mode, ret);
	return ret;
}

/**
 * rt5671_dsp_snd_effect - Set DSP sound effect.
 *
 * Set parameters of sound effect to DSP.
 *
 * Returns 0 for success or negative error code.
 */
static int rt5671_dsp_snd_effect(struct snd_soc_codec *codec)
{
	struct rt5671_priv *rt5671 = snd_soc_codec_get_drvdata(codec);

	snd_soc_update_bits(codec, RT5671_GEN_CTRL1, RT5671_RST_DSP,
		RT5671_RST_DSP);
	snd_soc_update_bits(codec, RT5671_GEN_CTRL1, RT5671_RST_DSP, 0);

	msleep(20);

	return rt5671_dsp_set_mode(codec, rt5671->dsp_sw);
}

static int rt5671_dsp_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *k, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_POST_PMD:
		dev_dbg(codec->dev, "%s(): PMD\n", __func__);
		rt5671_dsp_write(codec, 0x22f9, 1);
		break;

	case SND_SOC_DAPM_POST_PMU:
		dev_dbg(codec->dev, "%s(): PMU\n", __func__);
		rt5671_dsp_snd_effect(codec);
		break;

	default:
		return 0;
	}

	return 0;
}

static const struct snd_soc_dapm_widget rt5671_dsp_dapm_widgets[] = {
	SND_SOC_DAPM_SUPPLY_S("Voice DSP", 1, SND_SOC_NOPM,
		0, 0, rt5671_dsp_event,
		SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA("DSP Downstream", SND_SOC_NOPM,
		0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DSP Upstream", SND_SOC_NOPM,
		0, 0, NULL, 0),
};

static const struct snd_soc_dapm_route rt5671_dsp_dapm_routes[] = {
	{"DSP Downstream", NULL, "Voice DSP"},
	{"DSP Downstream", NULL, "RxDP Mux"},
	{"DSP Upstream", NULL, "Voice DSP"},
	{"DSP Upstream", NULL, "TDM Data Mux"},
	{"DSP DL Mux", "DSP", "DSP Downstream"},
	{"DSP UL Mux", "DSP", "DSP Upstream"},
};

/**
 * rt5671_dsp_show - Dump DSP registers.
 * @dev: codec device.
 * @attr: device attribute.
 * @buf: buffer for display.
 *
 * To show non-zero values of all DSP registers.
 *
 * Returns buffer length.
 */
static ssize_t rt5671_dsp_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5671_priv *rt5671 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5671->codec;
	unsigned short (*rt5671_dsp_tab)[2];
	unsigned int val;
	int cnt = 0, i, tab_num;

	switch (rt5671->dsp_sw) {
	case RT5671_DSP_NS:
		cnt += sprintf(buf, "[ DSP 'NS' ]\n");
		rt5671_dsp_tab = rt5671_dsp_ns;
		tab_num = RT5671_DSP_NS_NUM;
		break;

	case RT5671_DSP_DIS:
	default:
		cnt += sprintf(buf, "DSP Disabled\n");
		goto dsp_done;
	}

	for (i = 0; i < tab_num; i++) {
		if (cnt + RT5671_DSP_REG_DISP_LEN >= PAGE_SIZE)
			break;
		val = rt5671_dsp_read(codec, rt5671_dsp_tab[i][0]);
		cnt += snprintf(buf + cnt, RT5671_DSP_REG_DISP_LEN,
			"%04x: %04x\n", rt5671_dsp_tab[i][0], val);
	}

	if (cnt + RT5671_DSP_REG_DISP_LEN < PAGE_SIZE) {
		val = rt5671_dsp_read(codec, 0x3fb5);
		cnt += snprintf(buf + cnt, RT5671_DSP_REG_DISP_LEN,
			"%04x: %04x\n",
			0x3fb5, val);
	}
dsp_done:

	if (cnt >= PAGE_SIZE)
		cnt = PAGE_SIZE - 1;

	return cnt;
}

static ssize_t rt5671_dsp_reg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt5671_priv *rt5671 = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = rt5671->codec;
	unsigned int val = 0, addr = 0;
	int i;

	pr_debug("register \"%s\" count = %d\n", buf, count);

	/* address */
	for (i = 0; i < count; i++)
		if (*(buf + i) <= '9' && *(buf + i) >= '0')
			addr = (addr << 4) | (*(buf + i) - '0');
		else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
			addr = (addr << 4) | ((*(buf + i) - 'a') + 0xa);
		else if (*(buf + i) <= 'A' && *(buf + i) >= 'A')
			addr = (addr << 4) | ((*(buf + i) - 'A') + 0xa);
		else
			break;

	/* Value*/
	for (i = i + 1; i < count; i++)
		if (*(buf + i) <= '9' && *(buf + i) >= '0')
			val = (val << 4) | (*(buf + i) - '0');
		else if (*(buf + i) <= 'f' && *(buf + i) >= 'a')
			val = (val << 4) | ((*(buf + i) - 'a') + 0xa);
		else if (*(buf + i) <= 'F' && *(buf + i) >= 'A')
			val = (val << 4) | ((*(buf + i) - 'A') + 0xa);
		else
			break;

	pr_debug("addr=0x%x val=0x%x\n", addr, val);
	if (i == count)
		pr_debug("0x%04x = 0x%04x\n",
			addr, rt5671_dsp_read(codec, addr));
	else
		rt5671_dsp_write(codec, addr, val);

	return count;
}
static DEVICE_ATTR(dsp_reg, 0664, rt5671_dsp_show, rt5671_dsp_reg_store);

/**
 * rt5671_dsp_probe - register DSP for rt5671
 * @codec: audio codec
 *
 * To register DSP function for rt5671.
 *
 * Returns 0 for success or negative error code.
 */
int rt5671_dsp_probe(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);
	int ret;

	snd_soc_add_codec_controls(codec, rt5671_dsp_snd_controls,
			ARRAY_SIZE(rt5671_dsp_snd_controls));
	snd_soc_dapm_new_controls(dapm, rt5671_dsp_dapm_widgets,
			ARRAY_SIZE(rt5671_dsp_dapm_widgets));
	snd_soc_dapm_add_routes(dapm, rt5671_dsp_dapm_routes,
			ARRAY_SIZE(rt5671_dsp_dapm_routes));

	ret = device_create_file(codec->dev, &dev_attr_dsp_reg);
	if (ret != 0) {
		dev_err(codec->dev,
			"Failed to create index_reg sysfs files: %d\n", ret);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(rt5671_dsp_probe);

#ifdef CONFIG_PM
int rt5671_dsp_suspend(struct snd_soc_codec *codec)
{
	return 0;
}
EXPORT_SYMBOL_GPL(rt5671_dsp_suspend);

int rt5671_dsp_resume(struct snd_soc_codec *codec)
{
	return 0;
}
EXPORT_SYMBOL_GPL(rt5671_dsp_resume);
#endif

