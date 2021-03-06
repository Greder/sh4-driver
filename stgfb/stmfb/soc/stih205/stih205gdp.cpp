/***********************************************************************
 *
 * File: soc/stih205/stih205gdp.cpp
 * Copyright (c) 2008 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>
#include <Gamma/GenericGammaReg.h>

#include "stih205gdp.h"


CSTih205GDP::CSTih205GDP(stm_plane_id_t GDPid,
                         ULONG          baseAddr): CGammaCompositorGDP(GDPid, baseAddr, 0, true)
{
  DENTRY();

  m_bHasVFilter       = true;
  m_bHasFlickerFilter = true;

  m_ulMaxHSrcInc = m_fixedpointONE*4; // downscale to 1/4
  m_ulMinHSrcInc = m_fixedpointONE/8; // upscale 8x
  /* in reality, the hardware can downscale to 1/4 (vertical filter). This
     is achieved by the VF skipping lines for scales < 1/2, but the result
     just looks horrible. As such, we limit the downscale to 1/2 and any
     additional downscale is achieved by modifying the pitch. The question
     remains why the result looks (much!) better like that, as essentially
     the same should be happening in both cases. There is a bit in the
     vertical rescale engine to prevent the line skipping from happening for
     downscales < 1/2, but it does not seem to make any difference, and even
     if it did it would incur a non-neglegible increase in bandwidth as in
     that case the full plane has to be read, i.e. not doing the line skip
     will tremenduously increase the bandwidth requirement.
     As we can easily achieve bigger vertical rescales by adjusting the
     pitch, I would suggest we just continue doing it like this for the
     foreseeable future... */
  m_ulMaxVSrcInc = m_fixedpointONE*2; // downscale to 1/2
  m_ulMinVSrcInc = m_fixedpointONE/8;

  m_bHas4_13_precision = true;

  /*
   * Standard STBus transaction setup
   * TODO: Does this need to change for 1080p @50/60Hz?
   */
  g_pIOS->WriteRegister((volatile ULONG *)(m_GDPBaseAddr+GDPn_PAS) ,6); // 4K page
  g_pIOS->WriteRegister((volatile ULONG *)(m_GDPBaseAddr+GDPn_MAOS),5); // Max opcode = 32bytes
  g_pIOS->WriteRegister((volatile ULONG *)(m_GDPBaseAddr+GDPn_MIOS),3); // Min opcode = 8bytes
  g_pIOS->WriteRegister((volatile ULONG *)(m_GDPBaseAddr+GDPn_MACS),3); // Max chunk size = 8*max opcode size
  g_pIOS->WriteRegister((volatile ULONG *)(m_GDPBaseAddr+GDPn_MAMS),0); // Max message size = chunk size

  DEXIT();
}
