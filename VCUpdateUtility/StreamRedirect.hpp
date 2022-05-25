#pragma once
#include <make_exception.hpp>

#include <ostream>

struct StreamRedirect final {
private:
	std::ostream* target;
	std::streambuf* streambuf;
	bool isSwapped;

public:
	StreamRedirect() : target{ nullptr }, streambuf{ nullptr }, isSwapped{ false } {}
	StreamRedirect(std::ostream& os) : target{ &os }, streambuf{ nullptr }, isSwapped{ false } {}
	StreamRedirect(std::ostream& os, std::streambuf* alternate) : target{ &os }, streambuf{ target->rdbuf(alternate) }, isSwapped{ true } {}
	~StreamRedirect()
	{
		if (isSwapped && target != nullptr && streambuf != nullptr)
			target->rdbuf(streambuf);
	}

	void set_target(std::ostream& os) { target = &os; }
	void redirect(std::streambuf* alternate)
	{
		if (isSwapped) return;
		if (target == nullptr) throw make_exception("Cannot swap streambuffer to file descriptor; target was null!");

		streambuf = target->rdbuf(alternate);
	}
};

