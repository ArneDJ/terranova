
class ConsoleManager {
public:
	static void clear();
	static void print(const char *fmt, ...) IM_FMTARGS(2);
	static void display();
private:
	static ImVector<char*> m_items;
	static ImGuiTextFilter m_filter;
	static bool m_auto_scroll;
	static bool m_scroll_to_bottom;
};
