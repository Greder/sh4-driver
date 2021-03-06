/****************************************************************************
 *
 * LG 031 tuner driver
 * Copyright (C) ?
 *
 * Version for:
 * Edision argus VIP (1 pluggable tuner)
 * Edision argus VIP2 (2 pluggable tuners)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 ***************************************************************************/

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>

#include "dvb_frontend.h"
#include "lg031.h"

extern short paramDebug;  // debug print level is zero as default (0=nothing, 1= errors, 10=some detail, 20=more detail, 50=open/close functions, 100=all)
#if defined TAGDEBUG
#undef TAGDEBUG
#endif
#define TAGDEBUG "[lg031] "
#if !defined dprintk
#define dprintk(level, x...) \
do \
{ \
	if ((paramDebug) && (paramDebug >= level) || level == 0) \
	printk(TAGDEBUG x); \
} while (0)
#endif

struct lg031_state
{
	struct dvb_frontend       *fe;
	struct i2c_adapter        *i2c;
	const struct lg031_config *config;

	u32                       frequency;
	u32                       bandwidth;
	u32                       IF;
	u32                       TunerStep;
	unsigned char             IOBuffer[6];
};

static int lg031_read(struct lg031_state *state, u8 *buf)
{
	const struct lg031_config *config = state->config;
	int err = 0;
	struct i2c_msg msg = { .addr = config->addr, .flags = I2C_M_RD, .buf = buf, .len = 2 };

	err = i2c_transfer(state->i2c, &msg, 1);
	if (err != 1)
	{
		goto exit;
	}
	return err;

exit:
	dprintk(1, "%s: I/O Error err=<%d>\n", __func__, err);
	return err;
}

static int lg031_write(struct lg031_state *state, u8 *buf, u8 length)
{
	const struct lg031_config *config = state->config;
	int err = 0;
	struct i2c_msg msg = { .addr = config->addr, .flags = 0, .buf = buf, .len = length };

	err = i2c_transfer(state->i2c, &msg, 1);
	if (err != 1)
	{
		goto exit;
	}
	return err;

exit:
	dprintk(1, "%s: I/O Error err=<%d>\n", __func__, err);
	return err;
}

static int lg031_get_status(struct dvb_frontend *fe, u32 *status)
{
	struct lg031_state *state = fe->tuner_priv;
	u8 result[2] = {0};
	int err = 0;

	*status = 0;

	err = lg031_read(state, result);
	if (err < 0)
	{
		goto exit;
	}
	if (result[0] & 0x40)
	{
		dprintk(20, "%s: Tuner Phase Locked\n", __func__);
		*status = 1;
	}
	return err;

exit:
	dprintk(1, "%s: I/O Error\n", __func__);
	return err;
}

void tuner_lg031_CalWrBuffer(struct lg031_state *TunerConfig, u32  Frequency, u32  *NewFrequency)
{
	u32 uFreqPll;

	// calculate N0-N14
	// Note: ex)IF = 36.125 MHz
#if 1
	uFreqPll = ((Frequency) + TunerConfig->IF) * 1000;
	uFreqPll += TunerConfig->TunerStep / 2;
	uFreqPll /= TunerConfig->TunerStep;
#else
	uFreqPll = ((Frequency) + TunerConfig->IF) * 10;
    TunerConfig->TunerStep /= 100;
	uFreqPll /= TunerConfig->TunerStep;
#endif

	// real frequency programmed
	*NewFrequency = (uFreqPll * TunerConfig->TunerStep) - TunerConfig->IF * 1000;

	//-------------
	// byte 0 and 1	 // DB1, DB2
	//-------------
	// divider ratio
	TunerConfig->IOBuffer[0] = (u8)(uFreqPll >> 8);
	TunerConfig->IOBuffer[1] = (u8)uFreqPll;

	//-------
	// byte 2	// CB1
	//-------
	TunerConfig->IOBuffer[2] = 0x93;

	//-------
	// byte 3	// CB2
	//-------
	if( Frequency <= 143000 )  // 148
    {
        TunerConfig->IOBuffer[3] = 0x01;
    }
    else
    {
         if (Frequency <= 426000)  // 430
         {
            TunerConfig->IOBuffer[3] = 0x02;
         }
         else
         {
            TunerConfig->IOBuffer[3] = 0x08;
         }
    }
	//-------
	// byte 4	// AB
	//-------
	TunerConfig->IOBuffer[4] = 0xc3;  //0xc2
}

static int lg031_set_params(struct dvb_frontend* fe, struct dvb_frontend_parameters *params)
{
	struct lg031_state *state = fe->tuner_priv;
	int                err = 0;
	u32                status = 0;
	u32                f = params->frequency;
	u32	               NewFrequency;

	tuner_lg031_CalWrBuffer(state, f / 1000, &NewFrequency);

	/* open i2c repeater gate */
	if (fe->ops.i2c_gate_ctrl(fe, 1) < 0)
	{
		goto exit;
	}
	/* Set params */
	err = lg031_write(state, state->IOBuffer, 5);
	if (err < 0)
	{
		goto exit;
	}
	if (fe->ops.i2c_gate_ctrl(fe, 0) < 0)
	{
		goto exit;
	}
	if (fe->ops.i2c_gate_ctrl(fe, 1) < 0)
	{
		goto exit;
	}
	lg031_get_status(fe, &status);
	return 0;

exit:
	dprintk(1, "%s: I/O Error\n", __func__);
	return err;
}

static int lg031_release(struct dvb_frontend *fe)
{
	struct lg031_state *state = fe->tuner_priv;

	fe->tuner_priv = NULL;
	kfree(state);
	return 0;
}

static struct dvb_tuner_ops lg031_ops =
{
	.info =
	{
		.name           = "lg031",
		.frequency_step = 62500
	},
	.set_params         = lg031_set_params,
	.release            = lg031_release,
};

struct dvb_frontend *lg031_attach(struct dvb_frontend *fe, const struct lg031_config *config, struct i2c_adapter *i2c)
{
	struct lg031_state *state = NULL;
	struct dvb_tuner_info *info;

	state = kzalloc(sizeof(struct lg031_state), GFP_KERNEL);
	if (state == NULL)
	{
		goto exit;
	}
	state->config     = config;
	state->i2c        = i2c;
	state->fe         = fe;
	state->IF         = 36125;
	state->TunerStep  = 62500;
	fe->tuner_priv    = state;
	fe->ops.tuner_ops = lg031_ops;
	info              = &fe->ops.tuner_ops.info;

	memcpy(info->name, config->name, sizeof(config->name));
	dprintk(1, "%s: Attaching %s tuner\n", __func__, info->name);
	return fe;

exit:
	kfree(state);
	return NULL;
}
EXPORT_SYMBOL(lg031_attach);
// vim:ts=4
