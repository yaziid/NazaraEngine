// Copyright (C) 2014 Jérôme Leclercq
// This file is part of the "Nazara Engine - Graphics module"
// For conditions of distribution and use, see copyright notice in Config.hpp

#include <Nazara/Graphics/TextSprite.hpp>
#include <Nazara/Core/SparsePtr.hpp>
#include <Nazara/Graphics/AbstractViewer.hpp>
#include <memory>
#include <Nazara/Utility/Font.hpp>
#include <Nazara/Graphics/Debug.hpp>

///TODO: Gérer allocation nouvel atlas

NzTextSprite::NzTextSprite() :
m_boundingVolume(NzBoundingVolumef::Null()),
m_color(NzColor::White),
m_verticesUpdated(false)
{
	SetDefaultMaterial();
}

NzTextSprite::NzTextSprite(const NzTextSprite& sprite) :
NzSceneNode(sprite),
m_atlases(sprite.m_atlases),
m_renderInfos(sprite.m_renderInfos),
m_localVertices(sprite.m_localVertices),
m_vertices(sprite.m_vertices),
m_boundingVolume(sprite.m_boundingVolume),
m_color(sprite.m_color),
m_material(sprite.m_material),
m_localBounds(sprite.m_localBounds),
m_boundingVolumeUpdated(sprite.m_boundingVolumeUpdated),
m_verticesUpdated(sprite.m_verticesUpdated)
{
	SetParent(sprite.GetParent());

	for (const NzAbstractAtlas* atlas : m_atlases)
		atlas->AddListener(this);
}

NzTextSprite::~NzTextSprite()
{
	ClearAtlases();
}

void NzTextSprite::AddToRenderQueue(NzAbstractRenderQueue* renderQueue) const
{
	if (!m_verticesUpdated)
		UpdateVertices();

	for (auto& pair : m_renderInfos)
	{
		NzTexture* overlay = pair.first;
		RenderIndices& indices = pair.second;

		if (indices.count > 0)
			renderQueue->AddSprites(m_material, &m_vertices[indices.first*4], indices.count, overlay);
	}
}

void NzTextSprite::Clear()
{
	ClearAtlases();
	m_boundingVolume.MakeNull();
	m_localVertices.clear();
	m_renderInfos.clear();
	m_vertices.clear();
}

const NzBoundingVolumef& NzTextSprite::GetBoundingVolume() const
{
	if (!m_boundingVolumeUpdated)
		UpdateBoundingVolume();

	return m_boundingVolume;
}

const NzColor& NzTextSprite::GetColor() const
{
	return m_color;
}

NzMaterial* NzTextSprite::GetMaterial() const
{
	return m_material;
}

nzSceneNodeType NzTextSprite::GetSceneNodeType() const
{
	return nzSceneNodeType_TextSprite;
}

void NzTextSprite::InvalidateVertices()
{
	m_verticesUpdated = false;
}

bool NzTextSprite::IsDrawable() const
{
	return m_material != nullptr;
}

void NzTextSprite::SetColor(const NzColor& color)
{
	m_color = color;
	m_verticesUpdated = false;
}

void NzTextSprite::SetDefaultMaterial()
{
	std::unique_ptr<NzMaterial> material(new NzMaterial);
	material->Enable(nzRendererParameter_Blend, true);
	material->Enable(nzRendererParameter_DepthWrite, false);
	material->Enable(nzRendererParameter_FaceCulling, false);
	material->EnableLighting(false);
	material->SetDstBlend(nzBlendFunc_InvSrcAlpha);
	material->SetSrcBlend(nzBlendFunc_SrcAlpha);

	SetMaterial(material.get());

	material->SetPersistent(false);
	material.release();
}

void NzTextSprite::SetMaterial(NzMaterial* material)
{
	m_material = material;
}

void NzTextSprite::SetText(const NzAbstractTextDrawer& drawer)
{
	ClearAtlases();

	unsigned int fontCount = drawer.GetFontCount();
	for (unsigned int i = 0; i < fontCount; ++i)
	{
		const NzAbstractAtlas* atlas = drawer.GetFont(i)->GetAtlas();
		if (m_atlases.insert(atlas).second)
			atlas->AddListener(this);
	}

	unsigned int glyphCount = drawer.GetGlyphCount();
	m_localVertices.resize(glyphCount * 4);
	m_vertices.resize(glyphCount * 4);

	NzTexture* lastTexture = nullptr;
	unsigned int* count = nullptr;
	for (unsigned int i = 0; i < glyphCount; ++i)
	{
		const NzAbstractTextDrawer::Glyph& glyph = drawer.GetGlyph(i);

		NzTexture* texture = static_cast<NzTexture*>(glyph.atlas);
		if (lastTexture != texture)
		{
			auto pair = m_renderInfos.insert(std::make_pair(texture, RenderIndices{0U, 0U}));

			count = &pair.first->second.count;
			lastTexture = texture;
		}

		(*count)++;
	}

	// Attribution des indices
	unsigned int index = 0;
	for (auto& pair : m_renderInfos)
	{
		RenderIndices& indices = pair.second;

		indices.first = index;

		index += indices.count;
		indices.count = 0; // On réinitialise count à zéro (on va s'en servir pour compteur dans la boucle suivante)
	}

	NzSparsePtr<NzVector2f> texCoordPtr(&m_vertices[0].uv, sizeof(NzVertexStruct_XYZ_Color_UV));

	lastTexture = nullptr;
	RenderIndices* indices = nullptr;
	for (unsigned int i = 0; i < glyphCount; ++i)
	{
		const NzAbstractTextDrawer::Glyph& glyph = drawer.GetGlyph(i);

		NzTexture* texture = static_cast<NzTexture*>(glyph.atlas);
		if (lastTexture != texture)
		{
			indices = &m_renderInfos[texture]; // On a changé de texture, on ajuste le pointeur
			lastTexture = texture;
		}

		// Affectation des positions et couleurs (locaux)
		for (unsigned int j = 0; j < 4; ++j)
		{
			m_localVertices[i*4 + j].color = glyph.color;
			m_localVertices[i*4 + j].position.Set(glyph.corners[j]);
		}

		// Calcul des coordonnées de texture (globaux)

		// On commence par transformer les coordonnées entières en flottantes:
		NzVector2ui size(texture->GetSize());
		float invWidth = 1.f/size.x;
		float invHeight = 1.f/size.y;

		NzRectf uvRect(glyph.atlasRect);
		uvRect.x *= invWidth;
		uvRect.y *= invHeight;
		uvRect.width *= invWidth;
		uvRect.height *= invHeight;

		// Extraction des quatre coins et attribution
		NzSparsePtr<NzVector2f> texCoord = texCoordPtr + indices->first*4 + indices->count*4;
		if (!glyph.flipped)
		{
			// Le glyphe n'est pas retourné, l'ordre des UV suit celui des sommets
			*texCoord++ = uvRect.GetCorner(nzRectCorner_LeftTop);
			*texCoord++ = uvRect.GetCorner(nzRectCorner_RightTop);
			*texCoord++ = uvRect.GetCorner(nzRectCorner_LeftBottom);
			*texCoord++ = uvRect.GetCorner(nzRectCorner_RightBottom);
		}
		else
		{
			// Le glyphe a subit une rotation de 90° (sens antihoraire), on adapte les UV en conséquence
			*texCoord++ = uvRect.GetCorner(nzRectCorner_LeftBottom);
			*texCoord++ = uvRect.GetCorner(nzRectCorner_LeftTop);
			*texCoord++ = uvRect.GetCorner(nzRectCorner_RightBottom);
			*texCoord++ = uvRect.GetCorner(nzRectCorner_RightTop);
		}

		// Et on passe au prochain
		indices->count++;
	}

	m_localBounds = drawer.GetBounds();
	m_boundingVolume.MakeNull();
	m_boundingVolumeUpdated = false;
	m_verticesUpdated = false;
}

NzTextSprite& NzTextSprite::operator=(const NzTextSprite& text)
{
	NzSceneNode::operator=(text);

	m_atlases = text.m_atlases;
	m_color = text.m_color;
	m_material = text.m_material;
	m_renderInfos = text.m_renderInfos;
	m_localBounds = text.m_localBounds;
	m_localVertices = text.m_localVertices;

	// On ne copie pas les sommets finaux car il est très probable que nos paramètres soient modifiés et qu'ils doivent être régénérés de toute façon
	m_boundingVolumeUpdated = false;
	m_verticesUpdated = false;

	return *this;
}

void NzTextSprite::ClearAtlases()
{
	for (const NzAbstractAtlas* atlas : m_atlases)
		atlas->RemoveListener(this);

	m_atlases.clear();
}

void NzTextSprite::InvalidateNode()
{
	NzSceneNode::InvalidateNode();

	m_boundingVolumeUpdated = false;
	m_verticesUpdated = false;
}

bool NzTextSprite::OnAtlasCleared(const NzAbstractAtlas* atlas, void* userdata)
{
	NazaraUnused(userdata);

	#ifdef NAZARA_DEBUG
	if (m_atlases.find(atlas) == m_atlases.end())
	{
		NazaraInternalError("Not listening to " + NzString::Pointer(atlas));
		return false;
	}
	#endif

	NazaraWarning("TextSprite " + NzString::Pointer(this) + " has been cleared because atlas " + NzString::Pointer(atlas) + " that was under use has been cleared");
	Clear();

	return false;
}

bool NzTextSprite::OnAtlasLayerChange(const NzAbstractAtlas* atlas, NzAbstractImage* oldLayer, NzAbstractImage* newLayer, void* userdata)
{
	NazaraUnused(atlas);
	NazaraUnused(userdata);

	#ifdef NAZARA_DEBUG
	if (m_atlases.find(atlas) == m_atlases.end())
	{
		NazaraInternalError("Not listening to " + NzString::Pointer(atlas));
		return false;
	}
	#endif

	NzTexture* oldTexture = static_cast<NzTexture*>(oldLayer);
	NzTexture* newTexture = static_cast<NzTexture*>(newLayer);

	auto it = m_renderInfos.find(oldTexture);
	if (it != m_renderInfos.end())
	{
		// Nous utilisons bien cette texture, nous devons mettre à jour les coordonnées de texture
		RenderIndices indices = std::move(it->second);

		NzVector2ui oldSize(oldTexture->GetSize());
		NzVector2ui newSize(newTexture->GetSize());
		NzVector2f scale = NzVector2f(oldSize)/NzVector2f(newSize);

		NzSparsePtr<NzVector2f> texCoordPtr(&m_vertices[indices.first].uv, sizeof(NzVertexStruct_XYZ_Color_UV));
		for (unsigned int i = 0; i < indices.count; ++i)
		{
			for (unsigned int j = 0; j < 4; ++j)
				texCoordPtr[i*4 + j] *= scale;
		}

		// Nous enlevons l'ancienne texture et rajoutons la nouvelle à sa place (pour les mêmes indices)
		m_renderInfos.erase(it);
		m_renderInfos.insert(std::make_pair(newTexture, std::move(indices)));
	}

	return true;
}

void NzTextSprite::OnAtlasReleased(const NzAbstractAtlas* atlas, void* userdata)
{
	NazaraUnused(userdata);

	#ifdef NAZARA_DEBUG
	if (m_atlases.find(atlas) == m_atlases.end())
	{
		NazaraInternalError("Not listening to " + NzString::Pointer(atlas));
		return;
	}
	#endif

	NazaraWarning("TextSprite " + NzString::Pointer(this) + " has been cleared because atlas " + NzString::Pointer(atlas) + " that was under use has been released");
	Clear();
}

void NzTextSprite::Register()
{
	// Le changement de scène peut affecter les sommets
	m_verticesUpdated = false;
}

void NzTextSprite::Unregister()
{
}

void NzTextSprite::UpdateBoundingVolume() const
{
	if (m_boundingVolume.IsNull())
	{
		NzVector3f down = m_scene->GetDown();
		NzVector3f right = m_scene->GetRight();

		m_boundingVolume.Set(NzVector3f(0.f), static_cast<float>(m_localBounds.width)*right + static_cast<float>(m_localBounds.height)*down);
	}

	if (!m_transformMatrixUpdated)
		UpdateTransformMatrix();

	m_boundingVolume.Update(m_transformMatrix);
	m_boundingVolumeUpdated = true;
}

void NzTextSprite::UpdateVertices() const
{
	if (!m_transformMatrixUpdated)
		UpdateTransformMatrix();

	NzVector3f down = m_scene->GetDown();
	NzVector3f right = m_scene->GetRight();

	NzSparsePtr<NzColor> colorPtr(&m_vertices[0].color, sizeof(NzVertexStruct_XYZ_Color_UV));
	NzSparsePtr<NzVector3f> posPtr(&m_vertices[0].position, sizeof(NzVertexStruct_XYZ_Color_UV));

	for (auto& pair : m_renderInfos)
	{
		RenderIndices& indices = pair.second;

		NzSparsePtr<NzColor> color = colorPtr + indices.first*4;
		NzSparsePtr<NzVector3f> pos = posPtr + indices.first*4;
		NzVertexStruct_XY_Color* localVertex = &m_localVertices[indices.first*4];
		for (unsigned int i = 0; i < indices.count; ++i)
		{
			for (unsigned int j = 0; j < 4; ++j)
			{
				*pos++ = m_transformMatrix.Transform(localVertex->position.x*right + localVertex->position.y*down);
				*color++ = m_color * localVertex->color;

				localVertex++;
			}
		}
	}

	m_verticesUpdated = true;
}