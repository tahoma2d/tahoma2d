#if !defined(UTILS_INTERFACEHOLDER_HPP__)
#define UTILS_INTERFACEHOLDER_HPP__
#include <toonz_plugin.h>
#include <toonz_hostif.h>

extern toonz::host_interface_t *ifactory_;

void release_interf(void *interf)
{
	ifactory_->release_interface(interf);
}

template <class T>
std::unique_ptr<T, decltype(&release_interf)> grab_interf(const toonz::UUID *uuid)
{
	T *interf = nullptr;
	if (ifactory_->query_interface(uuid, reinterpret_cast<void **>(&interf)))
		return std::unique_ptr<T, decltype(&release_interf)>(nullptr, release_interf);
	return std::move(std::unique_ptr<T, decltype(&release_interf)>(interf, release_interf));
}

#endif
