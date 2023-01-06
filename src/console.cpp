#include <stdio.h>    
#include <cstdlib>
#include <vector>

#include "extern/imgui/imgui.h"
//#include <extern/imgui/backends/imgui_impl_opengl3.h>
//#include <extern/imgui/backends/imgui_impl_sdl.h>
#include "extern/imgui/imgui_impl_opengl3.h"
#include "extern/imgui/imgui_impl_sdl.h"

#include "console.h"

std::vector<std::string> Console::m_lines;
bool Console::m_auto_scroll = true;

void Console::clear()
{
	m_lines.clear();
}

void Console::print_line(const std::string &line)
{
	m_lines.push_back(line);
}

void Console::display()
{
	ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
	ImGui::Begin("Console");

	// Reserve enough left-over height for 1 separator + 1 input text
	const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
	ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar);

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
	for (const auto &line : m_lines) {
		ImGui::TextUnformatted(line.c_str());
	}

	if (m_auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
		ImGui::SetScrollHereY(1.f);
	}

	ImGui::PopStyleVar();
	ImGui::EndChild();
	ImGui::Separator();

	ImGui::End();
}

