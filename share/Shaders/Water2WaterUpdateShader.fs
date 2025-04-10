/***********************************************************************
Water2WaterUpdateShader - Shader to adjust the water surface height
based on the additive water texture.
Copyright (c) 2012-2025 Oliver Kreylos

This file is part of the Augmented Reality Sandbox (SARndbox).

The Augmented Reality Sandbox is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The Augmented Reality Sandbox is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License along
with the Augmented Reality Sandbox; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
***********************************************************************/

#extension GL_ARB_texture_rectangle : enable
#extension GL_ARB_draw_buffers : enable

uniform sampler2DRect bathymetrySampler;
uniform sampler2DRect snowSampler;
uniform sampler2DRect quantitySampler;
uniform sampler2DRect waterSampler;
uniform float snowLine;
uniform float snowMelt;

void main()
	{
	/* Calculate the bathymetry elevation at the center of this cell: */
	float b=(texture2DRect(bathymetrySampler,vec2(gl_FragCoord.x-1.0,gl_FragCoord.y-1.0)).r+
	         texture2DRect(bathymetrySampler,vec2(gl_FragCoord.x,gl_FragCoord.y-1.0)).r+
	         texture2DRect(bathymetrySampler,vec2(gl_FragCoord.x-1.0,gl_FragCoord.y)).r+
	         texture2DRect(bathymetrySampler,vec2(gl_FragCoord.xy)).r)*0.25;
	
	/* Get the old snow height at the cell center: */
	float s=texture2DRect(snowSampler,gl_FragCoord.xy).r;
	
	/* Get the old conserved quantity at the cell center: */
	vec3 q=texture2DRect(quantitySampler,gl_FragCoord.xy).rgb;
	
	/* Calculate the old water column height: */
	float hOld=q.x-b;
	
	/* Calculate the effective changes in water and snow level: */
	float precip=texture2DRect(waterSampler,gl_FragCoord.xy).r;
	float dWater=precip;
	float dSnow=precip*4.0; // Snow is four times fluffier than water
	
	/* Make it rain or snow depending on bathymetry elevation: */
	if(precip>0.0)
		{
		float waterSnowFactor=smoothstep(snowLine-0.25,snowLine+0.25,b);
		dWater=dWater*(1.0-waterSnowFactor);
		dSnow=dSnow*waterSnowFactor;
		}
	
	/* Melt snow into water: */
	snowMelt=min(snowMelt,s);
	dSnow=dSnow-snowMelt;
	dWater=dWater+snowMelt/4.0; // Snow is four times fluffier than water
	
	/* Update the snow height: */
	s=max(s+dSnow,0.0);
	
	/* Update the conserved quantities: */
	if(dWater>=0.0)
		{
		/* Update the water surface level: */
		q.x=(hOld+dWater)+b;
		
		/* Leave the partial discharges alone, as new water is added with zero velocity: */
		// ...
		}
	else
		{
		/* Update the water surface height: */
		float hNew=max(hOld+dWater,0.0);
		q.x=hNew+b;
		
		/* Update the partial discharges, as water is removed at current velocity: */
		q.yz=hOld>0.0?q.yz*(hNew/hOld):vec2(0.0,0.0);
		}
	
	/* Write the updated conserved quantity: */
	gl_FragData[0]=vec4(q,0.0);
	
	/* Write the updated snow height: */
	gl_FragData[1]=vec4(s,0.0,0.0,0.0);
	}
