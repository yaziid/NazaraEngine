// Copyright (C) 2012 J�r�me Leclercq
// This file is part of the "Nazara Engine".
// For conditions of distribution and use, see copyright notice in Config.hpp

#include <Nazara/Utility/SoftwareBuffer.hpp>
#include <Nazara/Core/Error.hpp>
#include <Nazara/Utility/Config.hpp>
#include <cstring>
#include <stdexcept>
#include <Nazara/Utility/Debug.hpp>

NzSoftwareBuffer::NzSoftwareBuffer(NzBuffer* parent, nzBufferType type) :
m_type(type)
{
	NazaraUnused(parent);
}

NzSoftwareBuffer::~NzSoftwareBuffer()
{
}

bool NzSoftwareBuffer::Create(unsigned int size, nzBufferUsage usage)
{
	NazaraUnused(usage);

	// Cette allocation est prot�g�e car sa taille d�pend directement de param�tres utilisateurs
	try
	{
		m_buffer = new nzUInt8[size];
	}
	catch (const std::exception& e)
	{
		NazaraError("Failed to allocate software buffer (" + NzString(e.what()) + ')');
		return false;
	}

	m_mapped = false;

	return true;
}

void NzSoftwareBuffer::Destroy()
{
	delete[] m_buffer;
}

bool NzSoftwareBuffer::Fill(const void* data, unsigned int offset, unsigned int size)
{
	#if NAZARA_UTILITY_SAFE
	if (m_mapped)
	{
		NazaraError("Buffer already mapped");
		return false;
	}
	#endif

	std::memcpy(&m_buffer[offset], data, size);

	return true;
}

void* NzSoftwareBuffer::GetPointer()
{
	return m_buffer;
}

bool NzSoftwareBuffer::IsHardware() const
{
	return false;
}

void* NzSoftwareBuffer::Map(nzBufferAccess access, unsigned int offset, unsigned int size)
{
	NazaraUnused(access);
	NazaraUnused(size);

	#if NAZARA_UTILITY_SAFE
	if (m_mapped)
	{
		NazaraError("Buffer already mapped");
		return nullptr;
	}
	#endif

	m_mapped = true;

	return &m_buffer[offset];
}

bool NzSoftwareBuffer::Unmap()
{
	#if NAZARA_UTILITY_SAFE
	if (!m_mapped)
	{
		NazaraError("Buffer not mapped");
		return true;
	}
	#endif

	m_mapped = false;

	return true;
}