/***********************************************************************
DepthImageRenderer - Class to centralize storage of raw or filtered
depth images on the GPU, and perform simple repetitive rendering tasks
such as rendering elevation values into a frame buffer.
Copyright (c) 2014-2025 Oliver Kreylos

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

#include "DepthImageRenderer.h"

#include <GL/gl.h>
#include <GL/GLMiscTemplates.h>
#include <GL/GLVertexArrayParts.h>
#include <GL/GLContextData.h>
#include <GL/Extensions/GLARBFragmentShader.h>
#include <GL/Extensions/GLARBTextureRectangle.h>
#include <GL/Extensions/GLARBTextureFloat.h>
#include <GL/Extensions/GLARBVertexBufferObject.h>
#include <GL/Extensions/GLARBVertexShader.h>
#include <GL/GLTransformationWrappers.h>
#include <Video/LensDistortion.h>

#include "TextureTracker.h"
#include "ShaderHelper.h"

/*********************************************
Methods of class DepthImageRenderer::DataItem:
*********************************************/

DepthImageRenderer::DataItem::DataItem(void)
	:vertexBuffer(0),indexBuffer(0),
	 depthTexture(0),depthTextureVersion(0)
	{
	/* Allocate the buffers and textures: */
	glGenBuffersARB(1,&vertexBuffer);
	glGenBuffersARB(1,&indexBuffer);
	glGenTextures(1,&depthTexture);
	}

DepthImageRenderer::DataItem::~DataItem(void)
	{
	/* Release all allocated buffers and textures: */
	glDeleteBuffersARB(1,&vertexBuffer);
	glDeleteBuffersARB(1,&indexBuffer);
	glDeleteTextures(1,&depthTexture);
	}

/***********************************
Methods of class DepthImageRenderer:
***********************************/

GLint DepthImageRenderer::bindDepthTexture(DepthImageRenderer::DataItem* dataItem,TextureTracker& textureTracker) const
	{
	/* Bind the depth image texture to the next available texture unit: */
	GLint unit=textureTracker.bindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->depthTexture);
	
	/* Check if the texture is outdated: */
	if(dataItem->depthTextureVersion!=depthImageVersion)
		{
		/* Upload the new depth texture: */
		glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB,0,depthImageSize,GL_LUMINANCE,GL_FLOAT,depthImage.getData<GLfloat>());
		
		/* Mark the depth texture as current: */
		dataItem->depthTextureVersion=depthImageVersion;
		}
	
	return unit;
	}

DepthImageRenderer::DepthImageRenderer(const Size& sDepthImageSize)
	:depthImageSize(sDepthImageSize),
	 depthImageVersion(0)
	{
	/* Initialize the depth image: */
	depthImage=Kinect::FrameBuffer(depthImageSize,depthImageSize[1]*depthImageSize[0]*sizeof(float));
	float* diPtr=depthImage.getData<float>();
	for(unsigned int i=depthImageSize[1]*depthImageSize[0];i>0;--i,++diPtr)
		*diPtr=0.0f;
	++depthImageVersion;
	}

void DepthImageRenderer::initContext(GLContextData& contextData) const
	{
	/* Initialize required OpenGL extensions: */
	GLARBFragmentShader::initExtension();
	GLARBTextureFloat::initExtension();
	GLARBTextureRectangle::initExtension();
	GLARBVertexBufferObject::initExtension();
	GLARBVertexShader::initExtension();
	Shader::initExtensions();
	TextureTracker::initExtensions();
	
	/* Create a data item and add it to the context: */
	DataItem* dataItem=new DataItem;
	contextData.addDataItem(this,dataItem);
	
	/* Upload the grid of template vertices into the vertex buffer: */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,dataItem->vertexBuffer);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB,depthImageSize[1]*depthImageSize[0]*sizeof(Vertex),0,GL_STATIC_DRAW_ARB);
	Vertex* vPtr=static_cast<Vertex*>(glMapBufferARB(GL_ARRAY_BUFFER_ARB,GL_WRITE_ONLY_ARB));
	if(lensDistortion.isIdentity())
		{
		/* Create uncorrected pixel positions: */
		for(unsigned int y=0;y<depthImageSize[1];++y)
			for(unsigned int x=0;x<depthImageSize[0];++x,++vPtr)
				{
				vPtr->position[0]=Scalar(x)+Scalar(0.5);
				vPtr->position[1]=Scalar(y)+Scalar(0.5);
				}
		}
	else
		{
		/* Create lens distortion-corrected pixel positions: */
		for(unsigned int y=0;y<depthImageSize[1];++y)
			for(unsigned int x=0;x<depthImageSize[0];++x,++vPtr)
				{
				/* Transform the depth-image point to depth tangent space: */
				LensDistortion::Point dp(LensDistortion::Scalar(x)+LensDistortion::Scalar(0.5),LensDistortion::Scalar(y)+LensDistortion::Scalar(0.5));
				LensDistortion::Point dtp=i2t.transform(dp);
				
				/* Undistort the point: */
				LensDistortion::Point utp=lensDistortion.undistort(dtp);
				
				/* Transform the undistorted tangent-space point to depth image space: */
				LensDistortion::Point up=t2i.transform(utp);
				
				/* Store the undistorted point: */
				vPtr->position[0]=Scalar(up[0]);
				vPtr->position[1]=Scalar(up[1]);
				}
		}
	glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,0);
	
	/* Upload the surface's triangle indices into the index buffer: */
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,dataItem->indexBuffer);
	glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB,(depthImageSize[1]-1)*depthImageSize[0]*2*sizeof(GLuint),0,GL_STATIC_DRAW_ARB);
	GLuint* iPtr=static_cast<GLuint*>(glMapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,GL_WRITE_ONLY_ARB));
	for(unsigned int y=1;y<depthImageSize[1];++y)
		for(unsigned int x=0;x<depthImageSize[0];++x,iPtr+=2)
			{
			iPtr[0]=GLuint(y*depthImageSize[0]+x);
			iPtr[1]=GLuint((y-1)*depthImageSize[0]+x);
			}
	glUnmapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,0);
	
	/* Initialize the depth image texture: */
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->depthTexture);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_S,GL_CLAMP);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_T,GL_CLAMP);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB,0,GL_LUMINANCE32F_ARB,depthImageSize,0,GL_LUMINANCE,GL_FLOAT,0);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
	
	/* Create the depth rendering shader: */
	dataItem->depthShader.addShader(compileVertexShader("SurfaceDepthShader"));
	dataItem->depthShader.addShader(compileFragmentShader("SurfaceDepthShader"));
	dataItem->depthShader.link();
	dataItem->depthShader.setUniformLocation("depthSampler");
	dataItem->depthShader.setUniformLocation("projectionModelviewDepthProjection");
	
	/* Create the elevation rendering shader: */
	dataItem->elevationShader.addShader(compileVertexShader("SurfaceElevationShader"));
	dataItem->elevationShader.addShader(compileFragmentShader("SurfaceElevationShader"));
	dataItem->elevationShader.link();
	dataItem->elevationShader.setUniformLocation("depthSampler");
	dataItem->elevationShader.setUniformLocation("basePlaneDic");
	dataItem->elevationShader.setUniformLocation("weightDic");
	dataItem->elevationShader.setUniformLocation("projectionModelviewDepthProjection");
	}

void DepthImageRenderer::setDepthProjection(const PTransform& newDepthProjection)
	{
	/* Set the depth unprojection matrix: */
	depthProjection=newDepthProjection;
	
	/* Convert the depth projection matrix to column-major OpenGL format: */
	GLfloat* dpmPtr=depthProjectionMatrix;
	for(int j=0;j<4;++j)
		for(int i=0;i<4;++i,++dpmPtr)
			*dpmPtr=GLfloat(depthProjection.getMatrix()(i,j));
	
	/* Create the weight calculation equation: */
	for(int i=0;i<4;++i)
		weightDicEq[i]=GLfloat(depthProjection.getMatrix()(3,i));
	
	/* Recalculate the base plane equation in depth image space: */
	setBasePlane(basePlane);
	}

void DepthImageRenderer::setIntrinsics(const Kinect::FrameSource::IntrinsicParameters& ips)
	{
	/* Set the lens distortion parameters: */
	lensDistortion=ips.depthLensDistortion;
	
	/* Set the depth unprojection matrix: */
	depthProjection=ips.depthProjection;
	
	/* Set the pixel space transformation matrices: */
	i2t=ips.di2t;
	t2i=ips.dt2i;
	
	/* Convert the depth projection matrix to column-major OpenGL format: */
	GLfloat* dpmPtr=depthProjectionMatrix;
	for(int j=0;j<4;++j)
		for(int i=0;i<4;++i,++dpmPtr)
			*dpmPtr=GLfloat(depthProjection.getMatrix()(i,j));
	
	/* Create the weight calculation equation: */
	for(int i=0;i<4;++i)
		weightDicEq[i]=GLfloat(depthProjection.getMatrix()(3,i));
	
	/* Recalculate the base plane equation in depth image space: */
	setBasePlane(basePlane);
	}

void DepthImageRenderer::setBasePlane(const Plane& newBasePlane)
	{
	/* Set the base plane: */
	basePlane=newBasePlane;
	
	/* Transform the base plane to depth image space and into a GLSL-compatible format: */
	const PTransform::Matrix& dpm=depthProjection.getMatrix();
	const Plane::Vector& bpn=basePlane.getNormal();
	Scalar bpo=basePlane.getOffset();
	for(int i=0;i<4;++i)
		basePlaneDicEq[i]=GLfloat(dpm(0,i)*bpn[0]+dpm(1,i)*bpn[1]+dpm(2,i)*bpn[2]-dpm(3,i)*bpo);
	}

void DepthImageRenderer::setDepthImage(const Kinect::FrameBuffer& newDepthImage)
	{
	/* Update the depth image: */
	depthImage=newDepthImage;
	++depthImageVersion;
	}

Scalar DepthImageRenderer::intersectLine(const Point& p0,const Point& p1,Scalar elevationMin,Scalar elevationMax) const
	{
	/* Initialize the line segment: */
	Scalar lambda0=Scalar(0);
	Scalar lambda1=Scalar(1);
	
	/* Intersect the line segment with the upper elevation plane: */
	Scalar d0=basePlane.calcDistance(p0);
	Scalar d1=basePlane.calcDistance(p1);
	if(d0*d1<Scalar(0))
		{
		/* Calculate the intersection parameter: */
		
		// IMPLEMENT ME!
		
		return Scalar(2);
		}
	else if(d1>Scalar(0))
		{
		/* Trivially reject with maximum intercept: */
		return Scalar(2);
		}
	
	return Scalar(2);
	}

void DepthImageRenderer::uploadDepthProjection(Shader& shader) const
	{
	/* Upload the matrix to OpenGL: */
	shader.uploadUniformMatrix4(1,GL_FALSE,depthProjectionMatrix);
	}

void DepthImageRenderer::renderSurfaceTemplate(GLContextData& contextData) const
	{
	/* Get the data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Bind the vertex and index buffers: */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,dataItem->vertexBuffer);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,dataItem->indexBuffer);
	
	/* Draw the surface template: */
	GLVertexArrayParts::enable(Vertex::getPartsMask());
	glVertexPointer(static_cast<const Vertex*>(0));
	GLuint* indexPtr=0;
	for(unsigned int y=1;y<depthImageSize[1];++y,indexPtr+=depthImageSize[0]*2)
		glDrawElements(GL_QUAD_STRIP,depthImageSize[0]*2,GL_UNSIGNED_INT,indexPtr);
	GLVertexArrayParts::disable(Vertex::getPartsMask());
	
	/* Unbind the vertex and index buffers: */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,0);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,0);
	}

void DepthImageRenderer::renderDepth(const PTransform& projectionModelview,GLContextData& contextData,TextureTracker& textureTracker) const
	{
	/* Get the data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Install the depth rendering shader: */
	dataItem->depthShader.use();
	textureTracker.reset();
	
	/* Bind the depth image texture to the depth rendering shader: */
	dataItem->depthShader.uploadUniform(bindDepthTexture(dataItem,textureTracker));
	
	/* Upload the combined projection, modelview, and depth projection matrix: */
	PTransform pmvdp=projectionModelview;
	pmvdp*=depthProjection;
	dataItem->depthShader.uploadUniform(pmvdp);
	
	/* Bind the vertex and index buffers: */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,dataItem->vertexBuffer);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,dataItem->indexBuffer);
	
	/* Draw the surface: */
	GLVertexArrayParts::enable(Vertex::getPartsMask());
	glVertexPointer(static_cast<const Vertex*>(0));
	GLuint* indexPtr=0;
	for(unsigned int y=1;y<depthImageSize[1];++y,indexPtr+=depthImageSize[0]*2)
		glDrawElements(GL_QUAD_STRIP,depthImageSize[0]*2,GL_UNSIGNED_INT,indexPtr);
	GLVertexArrayParts::disable(Vertex::getPartsMask());
	
	/* Unbind the vertex and index buffers: */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,0);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,0);
	}

void DepthImageRenderer::renderElevation(const PTransform& projectionModelview,GLContextData& contextData,TextureTracker& textureTracker) const
	{
	/* Get the data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Install the elevation rendering shader: */
	dataItem->elevationShader.use();
	textureTracker.reset();
	
	/* Bind the depth image texture to the elevation rendering shader: */
	dataItem->elevationShader.uploadUniform(bindDepthTexture(dataItem,textureTracker));
	
	/* Upload the base plane equation in depth image space: */
	dataItem->elevationShader.uploadUniform4v(1,basePlaneDicEq);
	
	/* Upload the base weight equation in depth image space: */
	dataItem->elevationShader.uploadUniform4v(1,weightDicEq);
	
	/* Upload the combined projection, modelview, and depth projection matrix: */
	PTransform pmvdp=projectionModelview;
	pmvdp*=depthProjection;
	dataItem->elevationShader.uploadUniform(pmvdp);
	
	/* Bind the vertex and index buffers: */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,dataItem->vertexBuffer);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,dataItem->indexBuffer);
	
	/* Draw the surface: */
	GLVertexArrayParts::enable(Vertex::getPartsMask());
	glVertexPointer(static_cast<const Vertex*>(0));
	GLuint* indexPtr=0;
	for(unsigned int y=1;y<depthImageSize[1];++y,indexPtr+=depthImageSize[0]*2)
		glDrawElements(GL_QUAD_STRIP,depthImageSize[0]*2,GL_UNSIGNED_INT,indexPtr);
	GLVertexArrayParts::disable(Vertex::getPartsMask());
	
	/* Unbind the vertex and index buffers: */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,0);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,0);
	}
