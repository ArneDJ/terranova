#include <stdio.h>    
#include <cstdlib>

#include "extern/imgui/imgui.h"
#include "extern/imgui/imgui_impl_sdl.h"
#include "extern/imgui/imgui_impl_opengl3.h"

#include "console.h"

ImVector<char*> ConsoleManager::m_items;
ImGuiTextFilter ConsoleManager::m_filter;
bool ConsoleManager::m_auto_scroll = true;
bool ConsoleManager::m_scroll_to_bottom = false;

static char* Strdup(const char* s)                           
{ 
	IM_ASSERT(s); 
	size_t len = strlen(s) + 1; 
	void* buf = malloc(len); 
	IM_ASSERT(buf); 

	return (char*)memcpy(buf, (const void*)s, len); 
}

void ConsoleManager::clear()
{
	for (int i = 0; i < m_items.Size; i++) {
	    free(m_items[i]);
	}
	m_items.clear();
}

void ConsoleManager::print(const char *fmt, ...) IM_FMTARGS(2)
{
	// FIXME-OPT
	char buf[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
	buf[IM_ARRAYSIZE(buf)-1] = 0;
	va_end(args);
	m_items.push_back(Strdup(buf));
}

void ConsoleManager::display()
{
	ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
	ImGui::Begin("Console");


	if (ImGui::SmallButton("Add Debug Text"))  { print("%d some text", m_items.Size); print("some more text"); print("display very important message here!"); }
	ImGui::SameLine();
	if (ImGui::SmallButton("Add Debug Error")) { print("[error] something went wrong"); }
	ImGui::SameLine();
	if (ImGui::SmallButton("Clear"))           { clear(); }
	ImGui::SameLine();

	ImGui::Separator();


	// Reserve enough left-over height for 1 separator + 1 input text
	const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
	ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar);
	if (ImGui::BeginPopupContextWindow())
	{
	    if (ImGui::Selectable("Clear")) clear();
	    ImGui::EndPopup();
	}

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
	for (int i = 0; i < m_items.Size; i++) {
	    const char* item = m_items[i];
	    if (!m_filter.PassFilter(item))
		continue;

	    // Normally you would store more information in your item than just a string.
	    // (e.g. make m_items[] an array of structure, store color/type etc.)
	    ImVec4 color;
	    bool has_color = false;
	    if (strstr(item, "[error]"))          { color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); has_color = true; }
	    else if (strncmp(item, "# ", 2) == 0) { color = ImVec4(1.0f, 0.8f, 0.6f, 1.0f); has_color = true; }
	    if (has_color)
		ImGui::PushStyleColor(ImGuiCol_Text, color);
	    ImGui::TextUnformatted(item);
	    if (has_color)
		ImGui::PopStyleColor();
	}

	if (m_scroll_to_bottom || (m_auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
	    ImGui::SetScrollHereY(1.0f);
	m_scroll_to_bottom = false;

	ImGui::PopStyleVar();
	ImGui::EndChild();
	ImGui::Separator();

	ImGui::End();
}
