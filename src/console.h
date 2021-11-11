
class Console {
public:
	Console();
	~Console();
public:
	void clear();
	void print(const char *fmt, ...) IM_FMTARGS(2);
	void display();
private:
	ImVector<char*> m_items;
	ImGuiTextFilter m_filter;
	bool m_auto_scroll = true;
	bool m_scroll_to_bottom = false;
};
