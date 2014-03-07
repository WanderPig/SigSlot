#include <sigslot/sigslot.h>
#include <iostream>

class Source {
public:
	mutable sigslot::signal<sigslot::multi_threaded_local> signal_zero;
	mutable sigslot::signal<sigslot::multi_threaded_local, bool> signal_bool;
	
	Source() = default;
	Source(Source &) = delete;
	Source(Source &&) = delete;

	void kerpling() {
		m_toggle = !m_toggle;
		signal_bool.emit(m_toggle);
	}
	
	void boioing() const {
		signal_zero.emit();
	}
private:
	bool m_toggle{true};
};

class Sink : public sigslot::has_slots<> {
public:
	Sink() = default;
	
	void slot_bool(bool b) {
		std::cout << "Signalled bool(" << (b ? "true" : "false" ) << ")" << std::endl;
	}
	
	void slot_void() {
		std::cout << "Signalled void." << std::endl;
	}
};

int main() {
	Source source;
	
	source.kerpling();
	source.boioing();

	{
		Sink sink;

		source.signal_zero.connect(&sink, &Sink::slot_void);
		source.signal_bool.connect(&sink, &Sink::slot_bool);

		source.kerpling();
		source.boioing();
	}
	
	source.kerpling();
	source.boioing();
	
	return 0;
}
