// Copyright (C) 2015 Jérôme Leclercq
// This file is part of the "Nazara Engine - Utility module"
// For conditions of distribution and use, see copyright notice in Config.hpp

#include <memory>
#include <Nazara/Utility/Debug.hpp>

template<typename... Args>
NzImageRef NzImage::New(Args&&... args)
{
	std::unique_ptr<NzImage> object(new NzImage(std::forward<Args>(args)...));
	object->SetPersistent(false);

	return object.release();
}

#include <Nazara/Utility/DebugOff.hpp>
