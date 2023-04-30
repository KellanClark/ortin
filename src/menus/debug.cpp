#include "menus/debug.hpp"
#include "imgui_memory_editor.h"

#include <fstream>

// NDS Components
#include "emulator/dma.hpp"
#include "emulator/timer.hpp"
#include "arm946e/arm946e.hpp"
#include "emulator/nds9/dsmath.hpp"
#include "arm7tdmi/arm7tdmi.hpp"
#include "emulator/nds7/rtc.hpp"
#include "emulator/nds7/spi.hpp"
#include "emulator/nds7/apu.hpp"

DebugMenu::DebugMenu() {
	showLogs = false;
	showArm9Debug = false;
	showArm7Debug = false;
	showMemEditor = false;
	showIoReg9 = false;
	showIoReg7 = false;

	arm9disasm.defaultSettings();
	arm7disasm.defaultSettings();
}

DebugMenu::~DebugMenu() {
	//
}

void DebugMenu::drawMenu() {
	if (ImGui::BeginMenu("Debug")) {
		ImGui::MenuItem("Logs", nullptr, &showLogs);
		ImGui::MenuItem("ARM9 CPU Status", nullptr, &showArm9Debug);
		ImGui::MenuItem("ARM7 CPU Status", nullptr, &showArm7Debug);
		ImGui::MenuItem("Memory", nullptr, &showMemEditor);
		ImGui::MenuItem("NDS9 IO", nullptr, &showIoReg9);
		ImGui::MenuItem("NDS7 IO", nullptr, &showIoReg7);

		ImGui::EndMenu();
	}
}

void DebugMenu::drawWindows() {
	if (showLogs) logsWindow();
	if (showArm9Debug) arm9DebugWindow();
	if (showArm7Debug) arm7DebugWindow();
	if (showMemEditor) memEditorWindow();
	if (showIoReg9) ioReg9Window();
	if (showIoReg7) ioReg7Window();
}

// A helper function for input boxes
u32 numberInput(const char *text, bool hex, u32 currentValue, u32 max) {
	char buf[128];
	sprintf(buf, hex ? "0x%X" : "%d", currentValue);
	ImGui::InputText(text, buf, 128, hex ? ImGuiInputTextFlags_CharsHexadecimal : ImGuiInputTextFlags_CharsDecimal);
	return (u32)std::clamp(strtoull(buf, NULL, hex ? 16 : 0), (unsigned long long)0, (unsigned long long)max);
}

void DebugMenu::logsWindow() {
	static bool shouldAutoscroll = true;

	ImGui::SetNextWindowSize(ImVec2(700, 600), ImGuiCond_FirstUseEver);
	ImGui::Begin("Logs", &showLogs);

	ImGui::Checkbox("Auto-scroll", &shouldAutoscroll);
	ImGui::SameLine();
	if (ImGui::Button("Clear Logs")) {
		ortin.nds.addThreadEvent(NDS::CLEAR_LOG);
	}
	ImGui::SameLine();
	if (ImGui::Button("Save Logs")) {
		std::ofstream systemLogFileStream{"log", std::ios::trunc};
		systemLogFileStream << ortin.nds.shared->log.str();
		systemLogFileStream.close();
	}

	ImGui::Checkbox("Log DMA9", &ortin.nds.nds9->dma->logDma);
	ImGui::Checkbox("Log DMA7", &ortin.nds.nds7->dma->logDma);
	ImGui::SameLine();
	ImGui::Checkbox("Log RTC", &ortin.nds.nds7->rtc->logRtc);
	ImGui::SameLine();
	ImGui::Checkbox("Log SPI", &ortin.nds.nds7->spi->logSpi);
	ImGui::SameLine();
	ImGui::Checkbox("Log Firmware", &ortin.nds.nds7->spi->firmware.logFirmware);
	ImGui::Checkbox("Log Gamecard", &ortin.nds.gamecard->logGamecard);

	if (ImGui::TreeNode("ARM9 Disassembler Options")) {
		ImGui::Checkbox("Show AL Condition", (bool *)&arm9disasm.options.showALCondition);
		ImGui::Checkbox("Always Show S Bit", (bool *)&arm9disasm.options.alwaysShowSBit);
		ImGui::Checkbox("Show Operands in Hex", (bool *)&arm9disasm.options.printOperandsHex);
		ImGui::Checkbox("Show Addresses in Hex", (bool *)&arm9disasm.options.printAddressesHex);
		ImGui::Checkbox("Simplify Register Names", (bool *)&arm9disasm.options.simplifyRegisterNames);
		ImGui::Checkbox("Simplify LDM and STM to PUSH and POP", (bool *)&arm9disasm.options.simplifyPushPop);
		ImGui::Checkbox("Use Alternative Stack Suffixes for LDM and STM", (bool *)&arm9disasm.options.ldmStmStackSuffixes);
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("ARM7 Disassembler Options")) {
		ImGui::Checkbox("Show AL Condition", (bool *)&arm7disasm.options.showALCondition);
		ImGui::Checkbox("Always Show S Bit", (bool *)&arm7disasm.options.alwaysShowSBit);
		ImGui::Checkbox("Show Operands in Hex", (bool *)&arm7disasm.options.printOperandsHex);
		ImGui::Checkbox("Show Addresses in Hex", (bool *)&arm7disasm.options.printAddressesHex);
		ImGui::Checkbox("Simplify Register Names", (bool *)&arm7disasm.options.simplifyRegisterNames);
		ImGui::Checkbox("Simplify LDM and STM to PUSH and POP", (bool *)&arm7disasm.options.simplifyPushPop);
		ImGui::Checkbox("Use Alternative Stack Suffixes for LDM and STM", (bool *)&arm7disasm.options.ldmStmStackSuffixes);
		ImGui::TreePop();
	}

	ImGui::Spacing();
	ImGui::Separator();

	if (ImGui::TreeNode("Log")) {
		ImGui::BeginChild("logS", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
		ImGui::TextUnformatted(ortin.nds.shared->log.str().c_str());
		if (shouldAutoscroll)
			ImGui::SetScrollHereY(1.0f);
		ImGui::EndChild();
		ImGui::TreePop();
	}

	ImGui::End();
}

void DebugMenu::arm9DebugWindow() {
	auto& cpu = ortin.nds.nds9->cpu;

	ImGui::SetNextWindowSize(ImVec2(760, 480));
	ImGui::Begin("ARM9 CPU Status", &showArm9Debug);

	if (ImGui::Button("Reset"))
		ortin.nds.addThreadEvent(NDS::RESET);
	ImGui::SameLine();
	if (ortin.nds.running) {
		if (ImGui::Button("Pause"))
			ortin.nds.addThreadEvent(NDS::STOP);
	} else {
		if (ImGui::Button("Unpause"))
			ortin.nds.addThreadEvent(NDS::START);
	}

	ImGui::Spacing();
	if (ImGui::Button("Step")) {
		ortin.nds.addThreadEvent(NDS::STEP_ARM9);
	}
	ImGui::Separator();

	// CPU Status
	{
		ImGui::BeginChild("ChildL#arm9", ImVec2(525, 0), false);

		std::string tmp = arm9disasm.disassemble(cpu->reg.R[15] - (cpu->reg.thumbMode ? 4 : 8), cpu->pipelineOpcode3, cpu->reg.thumbMode);
		ImGui::Text("Current Opcode:  %s", tmp.c_str());
		ImGui::Spacing();
		if (ImGui::BeginTable("registers9", 8, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV)) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("Current");
			ImGui::TableNextColumn();
			ImGui::Text("User/\nSystem");
			ImGui::TableNextColumn();
			ImGui::Text("FIQ");
			ImGui::TableNextColumn();
			ImGui::Text("IRQ");
			ImGui::TableNextColumn();
			ImGui::Text("Supervisor");
			ImGui::TableNextColumn();
			ImGui::Text("Abort");
			ImGui::TableNextColumn();
			ImGui::Text("Undefined");
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r0:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R[0]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r1:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R[1]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r2:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R[2]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r3:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R[3]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r4:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R[4]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r5:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R[5]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r6:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R[6]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r7:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R[7]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r8:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R[8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_usr[8 - 8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_fiq[8 - 8]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r9:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R[9]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_usr[9 - 8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_fiq[9 - 8]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r10:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R[10]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_usr[10 - 8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_fiq[10 - 8]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r11:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R[11]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_usr[11 - 8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_fiq[11 - 8]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r12:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R[12]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_usr[12 - 8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_fiq[12 - 8]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r13:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R[13]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_usr[13 - 8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_fiq[13 - 8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_irq[13 - 8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_svc[13 - 8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_abt[13 - 8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_und[13 - 8]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r14:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R[14]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_usr[14 - 8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_fiq[14 - 8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_irq[14 - 8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_svc[14 - 8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_abt[14 - 8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_und[14 - 8]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r15:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R[15]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("SPSR:");
			ImGui::TableSetColumnIndex(3);
			ImGui::Text("%08X", cpu->reg.R_fiq[7]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_irq[7]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_svc[7]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_abt[7]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_und[7]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("CPSR:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.CPSR);
			ImGui::TableNextRow();

			ImGui::EndTable();
		}
		ImGui::Spacing();
		bool n = cpu->reg.flagN;
		ImGui::Checkbox("N", &n);
		ImGui::SameLine();
		bool z = cpu->reg.flagZ;
		ImGui::Checkbox("Z", &z);
		ImGui::SameLine();
		bool c = cpu->reg.flagC;
		ImGui::Checkbox("C", &c);
		ImGui::SameLine();
		bool v = cpu->reg.flagV;
		ImGui::Checkbox("V", &v);
		ImGui::SameLine();
		bool q = cpu->reg.flagQ;
		ImGui::Checkbox("Q", &q);
		ImGui::SameLine();
		bool i = cpu->reg.irqDisable;
		ImGui::Checkbox("I", &i);
		ImGui::SameLine();
		bool f = cpu->reg.fiqDisable;
		ImGui::Checkbox("F", &f);
		ImGui::SameLine();
		bool t = cpu->reg.thumbMode;
		ImGui::Checkbox("T", &t);

		ImGui::EndChild();
	}

	ImGui::SameLine();

	// Breakpoints Window
	{
		static u32 breakpointAddress = 0;
		static int selectedBreakpoint = -1;
		static std::vector<u32> breakpoints {};
		ImGui::BeginChild("ChildR##bkpt9", ImVec2(210, ImGui::GetContentRegionAvail().y), true);

		ImGui::Text("Breakpoints");
		ImGui::Separator();

		ImGui::PushItemWidth(80);
		breakpointAddress = numberInput("##bkptin9", true, breakpointAddress, 0x0FFFFFFF);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button("Add Breakpoint")) {
			bool match = false;
			for (u32 i : breakpoints) {
				if (i == breakpointAddress)
					match = true;
			}

			if (!match) {
				cpu->addBreakpoint(breakpointAddress);
				breakpoints.push_back(breakpointAddress);
			}
		}

		if (ImGui::Button("Delete Selected")) {
			if (selectedBreakpoint != -1) {
				cpu->removeBreakpoint(breakpoints[selectedBreakpoint]);
				breakpoints.erase(breakpoints.begin() + selectedBreakpoint);
				selectedBreakpoint = -1;
			}
		}

		for (int i = 0; i < breakpoints.size(); i++) {
			if (ImGui::Selectable(fmt::format("{:0>7X}", breakpoints[i]).c_str(), selectedBreakpoint == i))
				selectedBreakpoint = i;
		}

		ImGui::EndChild();
	}

	ImGui::End();
}

void DebugMenu::arm7DebugWindow() {
	auto& cpu = ortin.nds.nds7->cpu;

	ImGui::SetNextWindowSize(ImVec2(760, 480));
	ImGui::Begin("ARM7 CPU Status", &showArm7Debug);

	if (ImGui::Button("Reset"))
		ortin.nds.addThreadEvent(NDS::RESET);
	ImGui::SameLine();
	if (ortin.nds.running) {
		if (ImGui::Button("Pause"))
			ortin.nds.addThreadEvent(NDS::STOP);
	} else {
		if (ImGui::Button("Unpause"))
			ortin.nds.addThreadEvent(NDS::START);
	}

	ImGui::Spacing();
	if (ImGui::Button("Step")) {
		ortin.nds.addThreadEvent(NDS::STEP_ARM7);
	}
	ImGui::Separator();

	// CPU Status
	{
		ImGui::BeginChild("ChildL#arm7", ImVec2(525, 0), false);

		std::string tmp = arm9disasm.disassemble(cpu->reg.R[15] - (cpu->reg.thumbMode ? 4 : 8), cpu->pipelineOpcode3, cpu->reg.thumbMode);
		ImGui::Text("Current Opcode:  %s", tmp.c_str());
		ImGui::Spacing();
		if (ImGui::BeginTable("registers7", 8, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV)) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("Current");
			ImGui::TableNextColumn();
			ImGui::Text("User/\nSystem");
			ImGui::TableNextColumn();
			ImGui::Text("FIQ");
			ImGui::TableNextColumn();
			ImGui::Text("IRQ");
			ImGui::TableNextColumn();
			ImGui::Text("Supervisor");
			ImGui::TableNextColumn();
			ImGui::Text("Abort");
			ImGui::TableNextColumn();
			ImGui::Text("Undefined");
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r0:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R[0]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r1:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R[1]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r2:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R[2]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r3:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R[3]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r4:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R[4]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r5:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R[5]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r6:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R[6]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r7:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R[7]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r8:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R[8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_usr[8 - 8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_fiq[8 - 8]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r9:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R[9]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_usr[9 - 8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_fiq[9 - 8]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r10:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R[10]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_usr[10 - 8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_fiq[10 - 8]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r11:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R[11]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_usr[11 - 8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_fiq[11 - 8]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r12:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R[12]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_usr[12 - 8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_fiq[12 - 8]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r13:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R[13]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_usr[13 - 8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_fiq[13 - 8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_irq[13 - 8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_svc[13 - 8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_abt[13 - 8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_und[13 - 8]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r14:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R[14]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_usr[14 - 8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_fiq[14 - 8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_irq[14 - 8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_svc[14 - 8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_abt[14 - 8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_und[14 - 8]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r15:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R[15]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("SPSR:");
			ImGui::TableSetColumnIndex(3);
			ImGui::Text("%08X", cpu->reg.R_fiq[7]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_irq[7]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_svc[7]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_abt[7]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.R_und[7]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("CPSR:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu->reg.CPSR);
			ImGui::TableNextRow();

			ImGui::EndTable();
		}
		ImGui::Spacing();
		bool n = cpu->reg.flagN;
		ImGui::Checkbox("N", &n);
		ImGui::SameLine();
		bool z = cpu->reg.flagZ;
		ImGui::Checkbox("Z", &z);
		ImGui::SameLine();
		bool c = cpu->reg.flagC;
		ImGui::Checkbox("C", &c);
		ImGui::SameLine();
		bool v = cpu->reg.flagV;
		ImGui::Checkbox("V", &v);
		ImGui::SameLine();
		bool i = cpu->reg.irqDisable;
		ImGui::Checkbox("I", &i);
		ImGui::SameLine();
		bool f = cpu->reg.fiqDisable;
		ImGui::Checkbox("F", &f);
		ImGui::SameLine();
		bool t = cpu->reg.thumbMode;
		ImGui::Checkbox("T", &t);

		ImGui::EndChild();
	}

	ImGui::SameLine();

	// Breakpoints Window
	{
		static u32 breakpointAddress = 0;
		static int selectedBreakpoint = -1;
		static std::vector<u32> breakpoints {};
		ImGui::BeginChild("ChildR##bkpt7", ImVec2(210, ImGui::GetContentRegionAvail().y), true);

		ImGui::Text("Breakpoints");
		ImGui::Separator();

		ImGui::PushItemWidth(80);
		breakpointAddress = numberInput("##bkptin7", true, breakpointAddress, 0x0FFFFFFF);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button("Add Breakpoint")) {
			bool match = false;
			for (u32 i : breakpoints) {
				if (i == breakpointAddress)
					match = true;
			}

			if (!match) {
				cpu->addBreakpoint(breakpointAddress);
				breakpoints.push_back(breakpointAddress);
			}
		}

		if (ImGui::Button("Delete Selected")) {
			if (selectedBreakpoint != -1) {
				cpu->removeBreakpoint(breakpoints[selectedBreakpoint]);
				breakpoints.erase(breakpoints.begin() + selectedBreakpoint);
				selectedBreakpoint = -1;
			}
		}

		for (int i = 0; i < breakpoints.size(); i++) {
			if (ImGui::Selectable(fmt::format("{:0>7X}", breakpoints[i]).c_str(), selectedBreakpoint == i))
				selectedBreakpoint = i;
		}

		ImGui::EndChild();
	}

	ImGui::End();
}

struct MemoryRegion {
	std::string name;
	u8 *pointer;
	int size;
	bool fakeEntry;
};

void DebugMenu::memEditorWindow() {
	static MemoryEditor memEditor;
	static int selectedRegion = 0;

	const static std::array<MemoryRegion, 23> memoryRegions = {{
		{"--Shared--", NULL, 0, true},
		{"PSRAM/Main Memory (4MB mirrored 0x2000000 to 0x3000000)", ortin.nds.shared->psram, 0x400000, false},
		{"Shared WRAM (32KB)", ortin.nds.shared->wram, 0x8000, false},

		{"--ARM9 Only--", NULL, 0, true},
		{"Instruction TCM (32KB)", ortin.nds.nds9->cpu->cp15.itcm, 0x8000, false},
		{"Data TCM (16KB)", ortin.nds.nds9->cpu->cp15.dtcm, 0x4000, false},
		{"Standard Palettes (2KB mirrored 0x5000000 to 0x6000000)", ortin.nds.ppu->pram, 0x800, false},
		{"OAM (2KB mirrored 0x6000000 to 0x7000000)", ortin.nds.ppu->oam, 0x800, false},
		{"ARM9 BIOS (4KB)", ortin.nds.nds9->bios, 0x1000, false},

		{"--ARM7 Only--", NULL, 0, true},
		{"ARM7 BIOS (16KB 0x0000000 to 0x0004000)", ortin.nds.nds7->bios, 0x4000, false},
		{"ARM7 WRAM (64KB mirrored 0x3800000 to 0x4000000)", ortin.nds.nds7->wram, 0x10000, false},

		{"--VRAM Banks--", NULL, 0, true},
		{"VRAM Bank A (128K)", ortin.nds.ppu->vramA, 0x20000},
		{"VRAM Bank B (128K)", ortin.nds.ppu->vramB, 0x20000},
		{"VRAM Bank C (128K)", ortin.nds.ppu->vramC, 0x20000},
		{"VRAM Bank D (128K)", ortin.nds.ppu->vramD, 0x20000},
		{"VRAM Bank E (64K)", ortin.nds.ppu->vramE, 0x10000},
		{"VRAM Bank F (16K)", ortin.nds.ppu->vramF, 0x4000},
		{"VRAM Bank G (16K)", ortin.nds.ppu->vramG, 0x4000},
		{"VRAM Bank H (32K)", ortin.nds.ppu->vramH, 0x8000},
		{"VRAM Bank I (16K)", ortin.nds.ppu->vramI, 0x4000},

		{"--Unmapped--", NULL, 0, true},
	}};

	ImGui::SetNextWindowSize(ImVec2(570, 400), ImGuiCond_FirstUseEver);
	ImGui::Begin("Memory Editor", &showMemEditor);

	if (ImGui::BeginCombo("Location", memoryRegions[selectedRegion].name.c_str())) {
		for (int i = 0; i < memoryRegions.size(); i++) {
			if (ImGui::MenuItem(memoryRegions[i].name.c_str()))
				if (!memoryRegions[i].fakeEntry)
					selectedRegion = i;
		}

		ImGui::EndCombo();
	}

	memEditor.DrawContents(memoryRegions[selectedRegion].pointer, memoryRegions[selectedRegion].size);

	ImGui::End();
}

enum InputType {
	TEXT_BOX,
	TEXT_BOX_HEX,
	CHECKBOX,
	COMBO,
	SPECIAL
};

struct IoField {
	std::string_view name;
	int startBit;
	int length;
	InputType type;
	const char *comboText;
};

struct IoRegister {
	std::string_view name;
	std::string_view description;
	u32 address;
	int size;
	bool readable;
	bool writable;
	int numFields;
	IoField *fields;
};

static const std::array<IoRegister, 95> registers9 = {{
	{"(A) DISPCNT", "LCD Control", 0x4000000, 4, true, true, 23, (IoField[]){
		{"BG Mode", 0, 3, TEXT_BOX},
		{"BG0 2D/3D Selection", 3, 1, COMBO, "2D\0"
											 "3D\0\0"},
		{"Tile OBJ Mapping", 4, 1, COMBO, "2D\0"
										  "1D\0\0"},
		{"Bitmap OBJ 2D-Dimension", 5, 1, COMBO, "128x512 dots\0"
												 "256x256 dots\0\0"},
		{"Bitmap OBJ Mapping", 6, 1, COMBO, "2D\0"
											"1D\0\0"},
		{"Forced Blank", 7, 1, CHECKBOX},
		{"Screen Display BG0", 8, 1, CHECKBOX},
		{"Screen Display BG1", 9, 1, CHECKBOX},
		{"Screen Display BG2", 10, 1, CHECKBOX},
		{"Screen Display BG3", 11, 1, CHECKBOX},
		{"Screen Display OBJ", 12, 1, CHECKBOX},
		{"Window 0 Display Flag", 13, 1, CHECKBOX},
		{"Window 1 Display Flag", 14, 1, CHECKBOX},
		{"OBJ Window Display Flag", 15, 1, CHECKBOX},
		{"Display Mode", 16, 2, COMBO, "Display off\0"
									   "Graphics Display\0"
									   "VRAM Display\0"
									   "Main Memory Display\0\0"},
		{"VRAM block", 18, 2, TEXT_BOX},
		{"Tile OBJ 1D-Boundary", 20, 2, TEXT_BOX}, // ?
		{"Bitmap OBJ 1D-Boundary", 22, 1, TEXT_BOX}, // ?
		{"OBJ Processing during H-Blank", 23, 1, CHECKBOX},
		{"Character Base", 24, 3, TEXT_BOX},
		{"Screen Base", 27, 3, TEXT_BOX},
		{"BG Extended Palettes", 30, 1, CHECKBOX},
		{"OBJ Extended Palettes", 31, 1, CHECKBOX}}},
	{"DISPSTAT", "Display Status and Interrupt Control", 0x4000004, 2, true, true, 7, (IoField[]){
		{"V-Blank", 0, 1, CHECKBOX},
		{"H-Blank", 1, 1, CHECKBOX},
		{"VCOUNT Compare Status", 2, 1, CHECKBOX},
		{"V-Blank IRQ", 3, 1, CHECKBOX},
		{"H-Blank IRQ", 4, 1, CHECKBOX},
		{"VCOUNT Compare IRQ", 5, 1, CHECKBOX},
		{"VCOUNT Compare Value", 7, 9, SPECIAL}}},
	{"VCOUNT", "Vertical Counter", 0x4000006, 2, true, false, 1, (IoField[]){
		{"Current Scanline (LY)", 0, 9, TEXT_BOX}}},
	{"(A) BG0CNT", "BG0 Control", 0x4000008, 2, true, true, 7, (IoField[]){
		{"BG Priority", 0, 2, TEXT_BOX},
		{"Character Base Block", 2, 4, TEXT_BOX},
		{"Mosaic", 6, 1, CHECKBOX},
		{"Colors/Palettes", 7, 1, COMBO, "16/16\0"
										 "256/1\0\0"},
		{"Screen Base Block", 8, 5, TEXT_BOX},
		{"Ext Palette Slot", 13, 1, COMBO, "Slot 0\0"
										   "Slot2\0\0"},
		{"Screen Size", 14, 2, TEXT_BOX}}},
	{"(A) BG1CNT", "BG1 Control", 0x400000A, 2, true, true, 7, (IoField[]){
		{"BG Priority", 0, 2, TEXT_BOX},
		{"Character Base Block", 2, 4, TEXT_BOX},
		{"Mosaic", 6, 1, CHECKBOX},
		{"Colors/Palettes", 7, 1, COMBO, "16/16\0"
										 "256/1\0\0"},
		{"Screen Base Block", 8, 5, TEXT_BOX},
		{"Ext Palette Slot", 13, 1, COMBO, "Slot 1\0"
										   "Slot3\0\0"},
		{"Screen Size", 14, 2, TEXT_BOX}}},
	{"(A) BG2CNT", "BG2 Control", 0x400000C, 2, true, true, 7, (IoField[]){
		{"BG Priority", 0, 2, TEXT_BOX},
		{"Character Base Block", 2, 4, TEXT_BOX},
		{"Mosaic", 6, 1, CHECKBOX},
		{"Colors/Palettes", 7, 1, COMBO, "16/16\0"
										 "256/1\0\0"},
		{"Screen Base Block", 8, 5, TEXT_BOX},
		{"Display Area Overflow", 13, 1, COMBO, "Transparent\0"
												"Wraparound\0\0"},
		{"Screen Size", 14, 2, TEXT_BOX}}},
	{"(A) BG3CNT", "BG3 Control", 0x400000E, 2, true, true, 7, (IoField[]){
		{"BG Priority", 0, 2, TEXT_BOX},
		{"Character Base Block", 2, 4, TEXT_BOX},
		{"Mosaic", 6, 1, CHECKBOX},
		{"Colors/Palettes", 7, 1, COMBO, "16/16\0"
										 "256/1\0\0"},
		{"Screen Base Block", 8, 5, TEXT_BOX},
		{"Display Area Overflow", 13, 1, COMBO, "Transparent\0"
												"Wraparound\0\0"},
		{"Screen Size", 14, 2, TEXT_BOX}}},
	{"(A) BG0HOFS", "BG0 X-Offset", 0x4000010, 2, false, true, 1, (IoField[]){
		{"Offset", 0, 9, TEXT_BOX}}},
	{"(A) BG0VOFS", "BG0 Y-Offset", 0x4000012, 2, false, true, 1, (IoField[]){
		{"Offset", 0, 9, TEXT_BOX}}},
	{"(A) BG1HOFS", "BG1 X-Offset", 0x4000014, 2, false, true, 1, (IoField[]){
		{"Offset", 0, 9, TEXT_BOX}}},
	{"(A) BG1VOFS", "BG1 Y-Offset", 0x4000016, 2, false, true, 1, (IoField[]){
		{"Offset", 0, 9, TEXT_BOX}}},
	{"(A) BG2HOFS", "BG2 X-Offset", 0x4000018, 2, false, true, 1, (IoField[]){
		{"Offset", 0, 9, TEXT_BOX}}},
	{"(A) BG2VOFS", "BG2 Y-Offset", 0x400001A, 2, false, true, 1, (IoField[]){
		{"Offset", 0, 9, TEXT_BOX}}},
	{"(A) BG3HOFS", "BG3 X-Offset", 0x400001C, 2, false, true, 1, (IoField[]){
		{"Offset", 0, 9, TEXT_BOX}}},
	{"(A) BG3VOFS", "BG3 Y-Offset", 0x400001E, 2, false, true, 1, (IoField[]){
		{"Offset", 0, 9, TEXT_BOX}}},
	{"(A) BG2PA", "BG2 Rotation/Scaling Parameter A (alias dx)", 0x4000020, 2, false, true, 2, (IoField[]){
		{"BG2 Rotation/Scaling Parameter A", 0, 16, TEXT_BOX_HEX},
		{"BG2 Rotation/Scaling Parameter A", 0, 16, SPECIAL}}},
	{"(A) BG2PB", "BG2 Rotation/Scaling Parameter B (alias dmx)", 0x4000022, 2, false, true, 2, (IoField[]){
		{"BG2 Rotation/Scaling Parameter B", 0, 16, TEXT_BOX_HEX},
		{"BG2 Rotation/Scaling Parameter B", 0, 16, SPECIAL}}},
	{"(A) BG2PC", "BG2 Rotation/Scaling Parameter C (alias dy)", 0x4000024, 2, false, true, 2, (IoField[]){
		{"BG2 Rotation/Scaling Parameter C", 0, 16, TEXT_BOX_HEX},
		{"BG2 Rotation/Scaling Parameter C", 0, 16, SPECIAL}}},
	{"(A) BG2PD", "BG2 Rotation/Scaling Parameter D (alias dmy)", 0x4000026, 2, false, true, 2, (IoField[]){
		{"BG2 Rotation/Scaling Parameter D", 0, 16, TEXT_BOX_HEX},
		{"BG2 Rotation/Scaling Parameter D", 0, 16, SPECIAL}}},
	{"(A) BG2X", "BG2 Reference Point X-Coordinate", 0x4000028, 4, false, true, 2, (IoField[]){
		{"BG2 Reference Point X-Coordinate", 0, 28, TEXT_BOX_HEX},
		{"BG2 Reference Point X-Coordinate", 0, 28, SPECIAL}}},
	{"(A) BG2Y", "BG2 Reference Point Y-Coordinate", 0x400002C, 4, false, true, 2, (IoField[]){
		{"BG2 Reference Point Y-Coordinate", 0, 28, TEXT_BOX_HEX},
		{"BG2 Reference Point Y-Coordinate", 0, 28, SPECIAL}}},
	{"(A) BG3PA", "BG3 Rotation/Scaling Parameter A (alias dx)", 0x4000030, 2, false, true, 2, (IoField[]){
		{"BG3 Rotation/Scaling Parameter A", 0, 16, TEXT_BOX_HEX},
		{"BG3 Rotation/Scaling Parameter A", 0, 16, SPECIAL}}},
	{"(A) BG3PB", "BG3 Rotation/Scaling Parameter B (alias dmx)", 0x4000032, 2, false, true, 2, (IoField[]){
		{"BG3 Rotation/Scaling Parameter B", 0, 16, TEXT_BOX_HEX},
		{"BG3 Rotation/Scaling Parameter B", 0, 16, SPECIAL}}},
	{"(A) BG3PC", "BG3 Rotation/Scaling Parameter C (alias dy)", 0x4000034, 2, false, true, 2, (IoField[]){
		{"BG3 Rotation/Scaling Parameter C", 0, 16, TEXT_BOX_HEX},
		{"BG3 Rotation/Scaling Parameter C", 0, 16, SPECIAL}}},
	{"(A) BG3PD", "BG3 Rotation/Scaling Parameter D (alias dmy)", 0x4000036, 2, false, true, 2, (IoField[]){
		{"BG3 Rotation/Scaling Parameter D", 0, 16, TEXT_BOX_HEX},
		{"BG3 Rotation/Scaling Parameter D", 0, 16, SPECIAL}}},
	{"(A) BG3X", "BG3 Reference Point X-Coordinate", 0x4000038, 4, false, true, 2, (IoField[]){
		{"BG3 Reference Point X-Coordinate", 0, 28, TEXT_BOX_HEX},
		{"BG3 Reference Point X-Coordinate", 0, 28, SPECIAL}}},
	{"(A) BG3Y", "BG3 Reference Point Y-Coordinate", 0x400003C, 4, false, true, 2, (IoField[]){
		{"BG3 Reference Point Y-Coordinate", 0, 28, TEXT_BOX_HEX},
		{"BG3 Reference Point Y-Coordinate", 0, 28, SPECIAL}}},
	{"(A) MOSAIC", "Mosaic Size", 0x400004C, 2, false, true, 4, (IoField[]){
		{"BG Mosaic H-Size (minus 1)", 0, 4, TEXT_BOX},
		{"BG Mosaic V-Size (minus 1)", 4, 4, TEXT_BOX},
		{"OBJ Mosaic H-Size (minus 1)", 8, 4, TEXT_BOX},
		{"OBJ Mosaic V-Size (minus 1)", 12, 4, TEXT_BOX}}},
	{"(A) MASTER_BRIGHT", "Master Brightness Up/Down", 0x400006C, 2, true, true, 2, (IoField[]){
		{"Factor used for 6bit R,G,B Intensities", 0, 5, TEXT_BOX},
		{"Mode", 14, 2, COMBO, "Disable\0"
							   "Up\0"
							   "Down\0"
							   "Reserved\0\0"}}},
	{"DMA0SAD", "DMA 0 Source Address", 0x40000B0, 4, true, true, 1, (IoField[]){
		{"DMA 0 Source Address", 0, 28, TEXT_BOX_HEX}}},
	{"DMA0DAD", "DMA 0 Destination Address", 0x40000B4, 4, true, true, 1, (IoField[]){
		{"DMA 0 Destination Address", 0, 28, TEXT_BOX_HEX}}},
	{"DMA0CNT", "DMA 0 Control", 0x40000B8, 4, true, true, 8, (IoField[]){
		{"Word Count", 0, 21, TEXT_BOX_HEX},
		{"Dest Addr Control", 21, 2, COMBO, "Increment\0"
											"Decrement\0"
											"Fixed\0"
											"Increment/Reload\0\0"},
		{"Source Adr Control", 23, 2, COMBO, "Increment\0"
											 "Decrement\0"
											 "Fixed\0"
											 "Prohibited\0\0"},
		{"DMA Repeat", 25, 1, CHECKBOX},
		{"DMA Transfer Type", 26, 1, CHECKBOX},
		{"DMA Start Timing", 27, 3, COMBO, "Start Immediately\0"
										   "Start at V-Blank\0"
										   "Start at H-Blank\0"
										   "Synchronize to start of display\0"
										   "Main memory display\0"
										   "DS Cartridge Slot\0"
										   "GBA Cartridge Slot\0"
										   "Geometry Command FIFO\0\0"},
		{"IRQ upon end of Word Count", 30, 1, CHECKBOX},
		{"DMA Enable", 31, 1, CHECKBOX}}},
	{"DMA1SAD", "DMA 1 Source Address", 0x40000BC, 4, true, true, 1, (IoField[]){
		{"DMA 0 Source Address", 0, 28, TEXT_BOX_HEX}}},
	{"DMA1DAD", "DMA 1 Destination Address", 0x40000C0, 4, true, true, 1, (IoField[]){
		{"DMA 0 Destination Address", 0, 28, TEXT_BOX_HEX}}},
	{"DMA1CNT", "DMA 1 Control", 0x40000C4, 4, true, true, 8, (IoField[]){
		{"Word Count", 0, 21, TEXT_BOX_HEX},
		{"Dest Addr Control", 21, 2, COMBO, "Increment\0"
											"Decrement\0"
											"Fixed\0"
											"Increment/Reload\0\0"},
		{"Source Adr Control", 23, 2, COMBO, "Increment\0"
											 "Decrement\0"
											 "Fixed\0"
											 "Prohibited\0\0"},
		{"DMA Repeat", 25, 1, CHECKBOX},
		{"DMA Transfer Type", 26, 1, CHECKBOX},
		{"DMA Start Timing", 27, 3, COMBO, "Start Immediately\0"
										   "Start at V-Blank\0"
										   "Start at H-Blank\0"
										   "Synchronize to start of display\0"
										   "Main memory display\0"
										   "DS Cartridge Slot\0"
										   "GBA Cartridge Slot\0"
										   "Geometry Command FIFO\0\0"},
		{"IRQ upon end of Word Count", 30, 1, CHECKBOX},
		{"DMA Enable", 31, 1, CHECKBOX}}},
	{"DMA2SAD", "DMA 2 Source Address", 0x40000C8, 4, true, true, 1, (IoField[]){
		{"DMA 0 Source Address", 0, 28, TEXT_BOX_HEX}}},
	{"DMA2DAD", "DMA 2 Destination Address", 0x40000CC, 4, true, true, 1, (IoField[]){
		{"DMA 0 Destination Address", 0, 28, TEXT_BOX_HEX}}},
	{"DMA2CNT", "DMA 2 Control", 0x40000D0, 4, true, true, 8, (IoField[]){
		{"Word Count", 0, 21, TEXT_BOX_HEX},
		{"Dest Addr Control", 21, 2, COMBO, "Increment\0"
											"Decrement\0"
											"Fixed\0"
											"Increment/Reload\0\0"},
		{"Source Adr Control", 23, 2, COMBO, "Increment\0"
											 "Decrement\0"
											 "Fixed\0"
											 "Prohibited\0\0"},
		{"DMA Repeat", 25, 1, CHECKBOX},
		{"DMA Transfer Type", 26, 1, CHECKBOX},
		{"DMA Start Timing", 27, 3, COMBO, "Start Immediately\0"
										   "Start at V-Blank\0"
										   "Start at H-Blank\0"
										   "Synchronize to start of display\0"
										   "Main memory display\0"
										   "DS Cartridge Slot\0"
										   "GBA Cartridge Slot\0"
										   "Geometry Command FIFO\0\0"},
		{"IRQ upon end of Word Count", 30, 1, CHECKBOX},
		{"DMA Enable", 31, 1, CHECKBOX}}},
	{"DMA3SAD", "DMA 3 Source Address", 0x40000D4, 4, true, true, 1, (IoField[]){
		{"DMA 0 Source Address", 0, 28, TEXT_BOX_HEX}}},
	{"DMA3DAD", "DMA 3 Destination Address", 0x40000D8, 4, true, true, 1, (IoField[]){
		{"DMA 0 Destination Address", 0, 28, TEXT_BOX_HEX}}},
	{"DMA3CNT", "DMA 3 Control", 0x40000DC, 4, true, true, 8, (IoField[]){
		{"Word Count", 0, 21, TEXT_BOX_HEX},
		{"Dest Addr Control", 21, 2, COMBO, "Increment\0"
											"Decrement\0"
											"Fixed\0"
											"Increment/Reload\0\0"},
		{"Source Adr Control", 23, 2, COMBO, "Increment\0"
											 "Decrement\0"
											 "Fixed\0"
											 "Prohibited\0\0"},
		{"DMA Repeat", 25, 1, CHECKBOX},
		{"DMA Transfer Type", 26, 1, CHECKBOX},
		{"DMA Start Timing", 27, 3, COMBO, "Start Immediately\0"
										   "Start at V-Blank\0"
										   "Start at H-Blank\0"
										   "Synchronize to start of display\0"
										   "Main memory display\0"
										   "DS Cartridge Slot\0"
										   "GBA Cartridge Slot\0"
										   "Geometry Command FIFO\0\0"},
		{"IRQ upon end of Word Count", 30, 1, CHECKBOX},
		{"DMA Enable", 31, 1, CHECKBOX}}},
	{"DMA0FILL", "DMA 0 Filldata", 0x40000E0, 4, true, true, 1, (IoField[]){
		{"DMA 0 Filldata", 0, 32, TEXT_BOX_HEX}}},
	{"DMA1FILL", "DMA 1 Filldata", 0x40000E4, 4, true, true, 1, (IoField[]){
		{"DMA 1 Filldata", 0, 32, TEXT_BOX_HEX}}},
	{"DMA2FILL", "DMA 2 Filldata", 0x40000E8, 4, true, true, 1, (IoField[]){
		{"DMA 2 Filldata", 0, 32, TEXT_BOX_HEX}}},
	{"DMA3FILL", "DMA 3 Filldata", 0x40000EC, 4, true, true, 1, (IoField[]){
		{"DMA 3 Filldata", 0, 32, TEXT_BOX_HEX}}},
	{"TM0CNT_L", "Timer 0 Counter/Reload", 0x4000100, 2, true, true, 1, (IoField[]){
		{"Counter(R)/Reload(W)", 0, 16, TEXT_BOX_HEX}}},
	{"TM0CNT_H", "Timer 0 Control", 0x4000102, 2, true, true, 3, (IoField[]){
		{"Prescaler Selection", 0, 2, COMBO, "F/1\0"
											 "F/64\0"
											 "F/256\0"
											 "F/1024\0\0"},
		{"Timer IRQ Enable", 6, 1, CHECKBOX},
		{"Timer Start/Stop", 7, 1, CHECKBOX}}},
	{"TM1CNT_L", "Timer 1 Counter/Reload", 0x4000104, 2, true, true, 1, (IoField[]){
		{"Counter(R)/Reload(W)", 0, 16, TEXT_BOX_HEX}}},
	{"TM1CNT_H", "Timer 1 Control", 0x4000106, 2, true, true, 4, (IoField[]){
		{"Prescaler Selection", 0, 2, COMBO, "F/1\0"
											 "F/64\0"
											 "F/256\0"
											 "F/1024\0\0"},
		{"Count-up Timing", 2, 1, CHECKBOX},
		{"Timer IRQ Enable", 6, 1, CHECKBOX},
		{"Timer Start/Stop", 7, 1, CHECKBOX}}},
	{"TM2CNT_L", "Timer 2 Counter/Reload", 0x4000108, 2, true, true, 1, (IoField[]){
		{"Counter(R)/Reload(W)", 0, 16, TEXT_BOX_HEX}}},
	{"TM2CNT_H", "Timer 2 Control", 0x400010A, 2, true, true, 4, (IoField[]){
		{"Prescaler Selection", 0, 2, COMBO, "F/1\0"
											 "F/64\0"
											 "F/256\0"
											 "F/1024\0\0"},
		{"Count-up Timing", 2, 1, CHECKBOX},
		{"Timer IRQ Enable", 6, 1, CHECKBOX},
		{"Timer Start/Stop", 7, 1, CHECKBOX}}},
	{"TM3CNT_L", "Timer 3 Counter/Reload", 0x400010C, 2, true, true, 1, (IoField[]){
		{"Counter(R)/Reload(W)", 0, 16, TEXT_BOX_HEX}}},
	{"TM3CNT_H", "Timer 4 Control", 0x400010E, 2, true, true, 4, (IoField[]){
		{"Prescaler Selection", 0, 2, COMBO, "F/1\0"
											 "F/64\0"
											 "F/256\0"
											 "F/1024\0\0"},
		{"Count-up Timing", 2, 1, CHECKBOX},
		{"Timer IRQ Enable", 6, 1, CHECKBOX},
		{"Timer Start/Stop", 7, 1, CHECKBOX}}},
	{"KEYINPUT", "Key Status (Inverted)", 0x4000130, 2, true, false, 10, (IoField[]){
		{"A", 0, 1, CHECKBOX},
		{"B", 1, 1, CHECKBOX},
		{"Select", 2, 1, CHECKBOX},
		{"Start", 3, 1, CHECKBOX},
		{"Right", 4, 1, CHECKBOX},
		{"Left", 5, 1, CHECKBOX},
		{"Up", 6, 1, CHECKBOX},
		{"Down", 7, 1, CHECKBOX},
		{"R", 8, 1, CHECKBOX},
		{"L", 9, 1, CHECKBOX}}},
	{"KEYCNT", "Key Interrupt Control", 0x4000132, 2, true, true, 12, (IoField[]){
		{"A", 0, 1, CHECKBOX},
		{"B", 1, 1, CHECKBOX},
		{"Select", 2, 1, CHECKBOX},
		{"Start", 3, 1, CHECKBOX},
		{"Right", 4, 1, CHECKBOX},
		{"Left", 5, 1, CHECKBOX},
		{"Up", 6, 1, CHECKBOX},
		{"Down", 7, 1, CHECKBOX},
		{"R", 8, 1, CHECKBOX},
		{"L", 9, 1, CHECKBOX},
		{"IRQ Enable", 14, 1, CHECKBOX},
		{"Condition", 15, 1, COMBO, "OR Mode\0"
									"AND Mode\0\0"}}},
	{"IPCSYNC", "IPC Synchronize Register", 0x4000180, 2, true, true, 4, (IoField[]){
		{"Data input from IPCSYNC of remote CPU", 0, 4, TEXT_BOX_HEX},
		{"Data output to IPCSYNC of remote CPU", 8, 4, TEXT_BOX_HEX},
		{"Send IRQ to remote CPU", 13, 1, CHECKBOX},
		{"Enable IRQ from remote CPU", 14, 1, CHECKBOX}}},
	{"IPCFIFOCNT", "IPC Fifo Control Register", 0x4000184, 2, true, true, 9, (IoField[]){
		{"Send Fifo Empty Status", 0, 1, CHECKBOX},
		{"Send Fifo Full Status", 1, 1, CHECKBOX},
		{"Send Fifo Empty IRQ", 2, 1, CHECKBOX},
		{"Send Fifo Clear", 3, 1, CHECKBOX},
		{"Receive Fifo Empty Status", 8, 1, CHECKBOX},
		{"Receive Fifo Full Status", 9, 1, CHECKBOX},
		{"Receive Fifo Not Empty IRQ", 10, 1, CHECKBOX},
		{"Error, Read Empty/Send Full", 14, 1, CHECKBOX},
		{"Enable Send/Receive Fifo", 15, 1, CHECKBOX}}},
	{"AUXSPICNT", "Gamecard ROM and SPI Control", 0x40001A0, 2, true, true, 6, (IoField[]){
		{"SPI Baudrate", 0, 2, COMBO, "4MHz/Default\0"
									  "2MHz\0"
									  "1MHz\0"
									  "512KHz\0\0"},
		{"SPI Hold Chipselect", 6, 1, CHECKBOX},
		{"SPI Busy", 7, 1, CHECKBOX},
		{"NDS Slot Mode", 13, 1, COMBO, "Parallel/ROM\0"
										"Serial/SPI-Backup\0\0"},
		{"Transfer Ready IRQ", 14, 1, CHECKBOX},
		{"NDS Slot Enable", 15, 1, CHECKBOX}}},
	{"ROMCTRL", "Gamecard Bus ROMCTRL", 0x40001A4, 4, true, true, 12, (IoField[]){
		{"KEY1 gap1 length", 0, 13, TEXT_BOX_HEX},
		{"KEY2 encrypt data", 13, 1, CHECKBOX},
		{"KEY2 Apply Seed", 15, 1, CHECKBOX},
		{"KEY1 gap2 length", 16, 6, TEXT_BOX_HEX},
		{"KEY2 encrypt cmd", 22, 1, CHECKBOX},
		{"Data-Word Status", 23, 1, CHECKBOX},
		{"Data Block size", 24, 3, COMBO, "None\0"
										  "200h bytes\0"
										  "400h bytes\0"
										  "800h bytes\0"
										  "1000h bytes\0"
										  "2000h bytes\0"
										  "4000h bytes\0"
										  "4 bytes\0\0"},
		{"Transfer CLK rate", 27, 1, COMBO, "6.7MHz=33.51MHz/5\0"
											"4.2MHz=33.51MHz/8\0\0"},
		{"KEY1 Gap CLKs", 28, 1, CHECKBOX},
		{"RESB Release Reset", 29, 1, CHECKBOX},
		{"Data Direction \"WR\"", 30, 1, CHECKBOX},
		{"Block Start/Status", 31, 1, CHECKBOX}}},
	{"Encryption Seed 0 Lower 32bit", "Encryption Seed 0 Lower 32bit", 0x40001B0, 4, false, true, 1, (IoField[]){
		{"Encryption Seed 0 Lower 32bit", 0, 32, TEXT_BOX_HEX}}},
	{"Encryption Seed 1 Lower 32bit", "Encryption Seed 1 Lower 32bit", 0x40001B4, 4, false, true, 1, (IoField[]){
		{"Encryption Seed 1 Lower 32bit", 0, 32, TEXT_BOX_HEX}}},
	{"Encryption Seed 0 Upper 7bit", "Encryption Seed 0 Upper 7bit", 0x40001B8, 2, false, true, 1, (IoField[]){
		{"Encryption Seed 0 Upper 7bit", 0, 7, TEXT_BOX_HEX}}},
	{"Encryption Seed 1 Upper 7bit", "Encryption Seed 1 Upper 7bit", 0x40001BA, 2, false, true, 1, (IoField[]){
		{"Encryption Seed 1 Upper 7bit", 0, 7, TEXT_BOX_HEX}}},
	{"EXMEMCNT","External Memory Control", 0x4000204, 2, true, true, 8, (IoField[]){
		{"32-pin GBA Slot SRAM Access Time", 0, 2, COMBO, "10 cycles\0"
														  "8 cycles\0"
														  "6 cycles\0"
														  "18 cycles\0\0"},
		{"32-pin GBA Slot ROM 1st Access Time", 2, 2, COMBO, "10 cycles\0"
															 "8 cycles\0"
															 "6 cycles\0"
															 "18 cycles\0\0"},
		{"32-pin GBA Slot ROM 2nd Access Time", 4, 1, COMBO, "6 cycles\0"
															 "4 cycles\0\0"},
		{"32-pin GBA Slot PHI-pin out", 5, 2, COMBO, "Low\0"
													 "4.19MHz\0"
													 "8.38MHz\0"
													 "16.76MHz\0\0"},
		{"32-pin GBA Slot Access Rights", 7, 1, COMBO, "ARM9\0"
													   "ARM7\0\0"},
		{"17-pin NDS Slot Access Rights", 11, 1, COMBO, "ARM9\0"
														"ARM7\0\0"},
		{"Main Memory Interface Mode Switch", 14, 1, COMBO, "Async/GBA/Reserved\0"
															"Synchronous\0\0"},
		{"Main Memory Access Priority", 15, 1, COMBO, "ARM9 Priority\0"
													  "ARM7 Priority\0\0"}}},
	{"IME", "Interrupt Master Enable", 0x4000208, 4, true, true, 1, (IoField[]){
		{"Enable Interrupts", 0, 1, CHECKBOX}}},
	{"IE", "Interrupt Enable", 0x4000210, 4, true, true, 19, (IoField[]){
		{"LCD V-BLank", 0, 1, CHECKBOX},
		{"LCD H-BLank", 1, 1, CHECKBOX},
		{"LCD V-Counter Match", 2, 1, CHECKBOX},
		{"Timer 0 Overflow", 3, 1, CHECKBOX},
		{"Timer 1 Overflow", 4, 1, CHECKBOX},
		{"Timer 2 Overflow", 5, 1, CHECKBOX},
		{"Timer 3 Overflow", 6, 1, CHECKBOX},
		{"DMA 0", 8, 1, CHECKBOX},
		{"DMA 1", 9, 1, CHECKBOX},
		{"DMA 2", 10, 1, CHECKBOX},
		{"DMA 3", 11, 1, CHECKBOX},
		{"Keypad", 12, 1, CHECKBOX},
		{"GBA Slot", 13, 1, CHECKBOX},
		{"IPC Sync", 16, 1, CHECKBOX},
		{"IPC Send FIFO Empty", 17, 1, CHECKBOX},
		{"IPC Recv FIFO Not Empty", 18, 1, CHECKBOX},
		{"NDS-Slot Game Card Data Transfer Completion", 19, 1, CHECKBOX},
		{"NDS-Slot Game Card IREQ_MC", 20, 1, CHECKBOX},
		{"Geometry Command FIFO", 21, 1, CHECKBOX}}},
	{"IF", "Interrupt Request Flags", 0x4000214, 4, true, true, 19, (IoField[]){
		{"LCD V-BLank", 0, 1, CHECKBOX},
		{"LCD H-BLank", 1, 1, CHECKBOX},
		{"LCD V-Counter Match", 2, 1, CHECKBOX},
		{"Timer 0 Overflow", 3, 1, CHECKBOX},
		{"Timer 1 Overflow", 4, 1, CHECKBOX},
		{"Timer 2 Overflow", 5, 1, CHECKBOX},
		{"Timer 3 Overflow", 6, 1, CHECKBOX},
		{"DMA 0", 8, 1, CHECKBOX},
		{"DMA 1", 9, 1, CHECKBOX},
		{"DMA 2", 10, 1, CHECKBOX},
		{"DMA 3", 11, 1, CHECKBOX},
		{"Keypad", 12, 1, CHECKBOX},
		{"GBA Slot", 13, 1, CHECKBOX},
		{"IPC Sync", 16, 1, CHECKBOX},
		{"IPC Send FIFO Empty", 17, 1, CHECKBOX},
		{"IPC Recv FIFO Not Empty", 18, 1, CHECKBOX},
		{"NDS-Slot Game Card Data Transfer Completion", 19, 1, CHECKBOX},
		{"NDS-Slot Game Card IREQ_MC", 20, 1, CHECKBOX},
		{"Geometry Command FIFO", 21, 1, CHECKBOX}}},
	{"VRAMCNT_A", "VRAM Bank A Control", 0x4000240, 1, false, true, 3, (IoField[]){
		{"MST", 0, 2, TEXT_BOX},
		{"Offset", 3, 2, TEXT_BOX},
		{"Enable", 7, 1, CHECKBOX}}},
	{"VRAMCNT_B", "VRAM Bank B Control", 0x4000241, 1, false, true, 3, (IoField[]){
		{"MST", 0, 2, TEXT_BOX},
		{"Offset", 3, 2, TEXT_BOX},
		{"Enable", 7, 1, CHECKBOX}}},
	{"VRAMCNT_C", "VRAM Bank C Control", 0x4000242, 1, false, true, 3, (IoField[]){
		{"MST", 0, 3, TEXT_BOX},
		{"Offset", 3, 2, TEXT_BOX},
		{"Enable", 7, 1, CHECKBOX}}},
	{"VRAMCNT_D", "VRAM Bank D Control", 0x4000243, 1, false, true, 3, (IoField[]){
		{"MST", 0, 3, TEXT_BOX},
		{"Offset", 3, 2, TEXT_BOX},
		{"Enable", 7, 1, CHECKBOX}}},
	{"VRAMCNT_E", "VRAM Bank E Control", 0x4000244, 1, false, true, 2, (IoField[]){
		{"MST", 0, 3, TEXT_BOX},
		{"Enable", 7, 1, CHECKBOX}}},
	{"VRAMCNT_F", "VRAM Bank F Control", 0x4000245, 1, false, true, 3, (IoField[]){
		{"MST", 0, 3, TEXT_BOX},
		{"Offset", 3, 2, TEXT_BOX},
		{"Enable", 7, 1, CHECKBOX}}},
	{"VRAMCNT_G", "VRAM Bank G Control", 0x4000246, 1, false, true, 3, (IoField[]){
		{"MST", 0, 3, TEXT_BOX},
		{"Offset", 3, 2, TEXT_BOX},
		{"Enable", 7, 1, CHECKBOX}}},
	{"WRAMCNT",	"WRAM Bank Control", 0x4000247, 1, true, true, 1, (IoField[]){
		{"Mapping", 0, 2, COMBO, "Full 32KB\0"
								 "Top 16KB\0"
								 "Bottom 16KB\0"
								 "Unmapped\0\0"}}},
	{"VRAMCNT_H", "VRAM Bank D Control", 0x4000248, 1, false, true, 2, (IoField[]){
		{"MST", 0, 2, TEXT_BOX},
		{"Enable", 7, 1, CHECKBOX}}},
	{"VRAMCNT_I", "VRAM Bank D Control", 0x4000249, 1, false, true, 2, (IoField[]){
		{"MST", 0, 2, TEXT_BOX},
		{"Enable", 7, 1, CHECKBOX}}},
	{"DIVCNT", "Division Control", 0x4000280, 2, true, true, 3, (IoField[]){
		{"Division Mode", 0, 2, COMBO, "32bit / 32bit = 32bit , 32bit\0"
									   "64bit / 32bit = 64bit , 32bit\0"
									   "64bit / 64bit = 64bit , 64bit\0"
									   "Reserved; same as Mode 1\0\0"},
		{"Division by zero", 14, 1, CHECKBOX},
		{"Busy", 15, 1, CHECKBOX}}},
	{"SQRTCNT", "Square Root Control", 0x40002B0, 2, true, true, 2, (IoField[]){
		{"Mode", 0, 1, COMBO, "32bit input\0"
							  "64bit input\0\0"},
		{"Busy", 15, 1, CHECKBOX}}},
	{"SQRT_RESULT", "Square Root Result", 0x40002B4, 4, true, false, 1, (IoField[]){
		{"Square Root Result", 0, 32, TEXT_BOX}}},
	{"(B) DISPCNT", "LCD Control", 0x4001000, 4, true, true, 18, (IoField[]){
		{"BG Mode", 0, 3, TEXT_BOX},
		{"Tile OBJ Mapping", 4, 1, COMBO, "2D\0"
										  "1D\0\0"},
		{"Bitmap OBJ 2D-Dimension", 5, 1, COMBO, "128x512 dots\0"
												 "256x256 dots\0\0"},
		{"Bitmap OBJ Mapping", 6, 1, COMBO, "2D\0"
											"1D\0\0"},
		{"Forced Blank", 7, 1, CHECKBOX},
		{"Screen Display BG0", 8, 1, CHECKBOX},
		{"Screen Display BG1", 9, 1, CHECKBOX},
		{"Screen Display BG2", 10, 1, CHECKBOX},
		{"Screen Display BG3", 11, 1, CHECKBOX},
		{"Screen Display OBJ", 12, 1, CHECKBOX},
		{"Window 0 Display Flag", 13, 1, CHECKBOX},
		{"Window 1 Display Flag", 14, 1, CHECKBOX},
		{"OBJ Window Display Flag", 15, 1, CHECKBOX},
		{"Display Mode", 16, 1, COMBO, "Display off\0"
									   "Graphics Display\0\0"},
		{"Tile OBJ 1D-Boundary", 20, 2, TEXT_BOX}, // ?
		{"OBJ Processing during H-Blank", 23, 1, CHECKBOX},
		{"BG Extended Palettes", 30, 1, CHECKBOX},
		{"OBJ Extended Palettes", 31, 1, CHECKBOX}}},
	{"(B) BG0CNT", "BG0 Control", 0x4001008, 2, true, true, 7, (IoField[]){
		{"BG Priority", 0, 2, TEXT_BOX},
		{"Character Base Block", 2, 4, TEXT_BOX},
		{"Mosaic", 6, 1, CHECKBOX},
		{"Colors/Palettes", 7, 1, COMBO, "16/16\0"
										 "256/1\0\0"},
		{"Screen Base Block", 8, 5, TEXT_BOX},
		{"Ext Palette Slot", 13, 1, COMBO, "Slot 0\0"
										   "Slot2\0\0"},
		{"Screen Size", 14, 2, TEXT_BOX}}},
	{"(B) BG1CNT", "BG1 Control", 0x400100A, 2, true, true, 7, (IoField[]){
		{"BG Priority", 0, 2, TEXT_BOX},
		{"Character Base Block", 2, 4, TEXT_BOX},
		{"Mosaic", 6, 1, CHECKBOX},
		{"Colors/Palettes", 7, 1, COMBO, "16/16\0"
										 "256/1\0\0"},
		{"Screen Base Block", 8, 5, TEXT_BOX},
		{"Ext Palette Slot", 13, 1, COMBO, "Slot 1\0"
										   "Slot3\0\0"},
		{"Screen Size", 14, 2, TEXT_BOX}}},
	{"(B) BG2CNT", "BG2 Control", 0x400100C, 2, true, true, 7, (IoField[]){
		{"BG Priority", 0, 2, TEXT_BOX},
		{"Character Base Block", 2, 4, TEXT_BOX},
		{"Mosaic", 6, 1, CHECKBOX},
		{"Colors/Palettes", 7, 1, COMBO, "16/16\0"
										 "256/1\0\0"},
		{"Screen Base Block", 8, 5, TEXT_BOX},
		{"Display Area Overflow", 13, 1, COMBO, "Transparent\0"
												"Wraparound\0\0"},
		{"Screen Size", 14, 2, TEXT_BOX}}},
	{"(B) BG3CNT", "BG3 Control", 0x400100E, 2, true, true, 7, (IoField[]){
		{"BG Priority", 0, 2, TEXT_BOX},
		{"Character Base Block", 2, 4, TEXT_BOX},
		{"Mosaic", 6, 1, CHECKBOX},
		{"Colors/Palettes", 7, 1, COMBO, "16/16\0"
										 "256/1\0\0"},
		{"Screen Base Block", 8, 5, TEXT_BOX},
		{"Display Area Overflow", 13, 1, COMBO, "Transparent\0"
												"Wraparound\0\0"},
		{"Screen Size", 14, 2, TEXT_BOX}}},
	{"(B) BG0HOFS", "BG0 X-Offset", 0x4001010, 2, false, true, 1, (IoField[]){
		{"Offset", 0, 9, TEXT_BOX}}},
	{"(B) BG0VOFS", "BG0 Y-Offset", 0x4001012, 2, false, true, 1, (IoField[]){
		{"Offset", 0, 9, TEXT_BOX}}},
	{"(B) BG1HOFS", "BG1 X-Offset", 0x4001014, 2, false, true, 1, (IoField[]){
		{"Offset", 0, 9, TEXT_BOX}}},
	{"(B) BG1VOFS", "BG1 Y-Offset", 0x4001016, 2, false, true, 1, (IoField[]){
		{"Offset", 0, 9, TEXT_BOX}}},
	{"(B) BG2HOFS", "BG2 X-Offset", 0x4001018, 2, false, true, 1, (IoField[]){
		{"Offset", 0, 9, TEXT_BOX}}},
	{"(B) BG2VOFS", "BG2 Y-Offset", 0x400101A, 2, false, true, 1, (IoField[]){
		{"Offset", 0, 9, TEXT_BOX}}},
	{"(B) BG3HOFS", "BG3 X-Offset", 0x400101C, 2, false, true, 1, (IoField[]){
		{"Offset", 0, 9, TEXT_BOX}}},
	{"(B) BG3VOFS", "BG3 Y-Offset", 0x400101E, 2, false, true, 1, (IoField[]){
		{"Offset", 0, 9, TEXT_BOX}}},
	{"(B) MOSAIC", "Mosaic Size", 0x400104C, 2, false, true, 4, (IoField[]){
		{"BG Mosaic H-Size (minus 1)", 0, 4, TEXT_BOX},
		{"BG Mosaic V-Size (minus 1)", 4, 4, TEXT_BOX},
		{"OBJ Mosaic H-Size (minus 1)", 8, 4, TEXT_BOX},
		{"OBJ Mosaic V-Size (minus 1)", 12, 4, TEXT_BOX}}},
	{"(B) MASTER_BRIGHT", "Master Brightness Up/Down", 0x400106C, 2, true, true, 2, (IoField[]){
		{"Factor used for 6bit R,G,B Intensities", 0, 5, TEXT_BOX},
		{"Mode", 14, 2, COMBO, "Disable\0"
							   "Up\0"
							   "Down\0"
							   "Reserved\0\0"}}},
}};

void DebugMenu::ioReg9Window() { // Shamefully stolen from the ImGui demo
	static std::array<void *, 95> registerPointers9 = {
		&ortin.nds.ppu->engineA.DISPCNT,
		&ortin.nds.ppu->DISPSTAT9,
		&ortin.nds.ppu->VCOUNT,
		&ortin.nds.ppu->engineA.bg[0].BGCNT,
		&ortin.nds.ppu->engineA.bg[1].BGCNT,
		&ortin.nds.ppu->engineA.bg[2].BGCNT,
		&ortin.nds.ppu->engineA.bg[3].BGCNT,
		&ortin.nds.ppu->engineA.bg[0].BGHOFS,
		&ortin.nds.ppu->engineA.bg[0].BGVOFS,
		&ortin.nds.ppu->engineA.bg[1].BGHOFS,
		&ortin.nds.ppu->engineA.bg[1].BGVOFS,
		&ortin.nds.ppu->engineA.bg[2].BGHOFS,
		&ortin.nds.ppu->engineA.bg[2].BGVOFS,
		&ortin.nds.ppu->engineA.bg[3].BGHOFS,
		&ortin.nds.ppu->engineA.bg[3].BGVOFS,
		&ortin.nds.ppu->engineA.bg[2].BGPA,
		&ortin.nds.ppu->engineA.bg[2].BGPB,
		&ortin.nds.ppu->engineA.bg[2].BGPC,
		&ortin.nds.ppu->engineA.bg[2].BGPD,
		&ortin.nds.ppu->engineA.bg[2].BGX,
		&ortin.nds.ppu->engineA.bg[2].BGY,
		&ortin.nds.ppu->engineA.bg[3].BGPA,
		&ortin.nds.ppu->engineA.bg[3].BGPB,
		&ortin.nds.ppu->engineA.bg[3].BGPC,
		&ortin.nds.ppu->engineA.bg[3].BGPD,
		&ortin.nds.ppu->engineA.bg[3].BGX,
		&ortin.nds.ppu->engineA.bg[3].BGY,
		&ortin.nds.ppu->engineA.MOSAIC,
		&ortin.nds.ppu->engineA.MASTER_BRIGHT,
		&ortin.nds.nds9->dma->channel[0].DMASAD,
		&ortin.nds.nds9->dma->channel[0].DMADAD,
		&ortin.nds.nds9->dma->channel[0].DMACNT,
		&ortin.nds.nds9->dma->channel[1].DMASAD,
		&ortin.nds.nds9->dma->channel[1].DMADAD,
		&ortin.nds.nds9->dma->channel[1].DMACNT,
		&ortin.nds.nds9->dma->channel[2].DMASAD,
		&ortin.nds.nds9->dma->channel[2].DMADAD,
		&ortin.nds.nds9->dma->channel[2].DMACNT,
		&ortin.nds.nds9->dma->channel[3].DMASAD,
		&ortin.nds.nds9->dma->channel[3].DMADAD,
		&ortin.nds.nds9->dma->channel[3].DMACNT,
		&ortin.nds.nds9->dma->DMA0FILL,
		&ortin.nds.nds9->dma->DMA1FILL,
		&ortin.nds.nds9->dma->DMA2FILL,
		&ortin.nds.nds9->dma->DMA3FILL,
		&ortin.nds.nds9->timer->timer[0].TIMCNT_L,
		&ortin.nds.nds9->timer->timer[0].TIMCNT_H,
		&ortin.nds.nds9->timer->timer[1].TIMCNT_L,
		&ortin.nds.nds9->timer->timer[1].TIMCNT_H,
		&ortin.nds.nds9->timer->timer[2].TIMCNT_L,
		&ortin.nds.nds9->timer->timer[2].TIMCNT_H,
		&ortin.nds.nds9->timer->timer[3].TIMCNT_L,
		&ortin.nds.nds9->timer->timer[3].TIMCNT_H,
		&ortin.nds.shared->KEYINPUT,
		&ortin.nds.shared->KEYCNT9,
		&ortin.nds.ipc->IPCSYNC9,
		&ortin.nds.ipc->IPCFIFOCNT9,
		&ortin.nds.gamecard->AUXSPICNT,
		&ortin.nds.gamecard->ROMCTRL,
		&ortin.nds.gamecard->key2Seed0Low7,
		&ortin.nds.gamecard->key2Seed1Low7,
		&ortin.nds.gamecard->key2Seed0High7,
		&ortin.nds.gamecard->key2Seed1High7,
		&ortin.nds.shared->EXMEMCNT,
		&ortin.nds.nds9->IME,
		&ortin.nds.nds9->IE,
		&ortin.nds.nds9->IF,
		&ortin.nds.ppu->VRAMCNT_A,
		&ortin.nds.ppu->VRAMCNT_B,
		&ortin.nds.ppu->VRAMCNT_C,
		&ortin.nds.ppu->VRAMCNT_D,
		&ortin.nds.ppu->VRAMCNT_E,
		&ortin.nds.ppu->VRAMCNT_F,
		&ortin.nds.ppu->VRAMCNT_G,
		&ortin.nds.shared->WRAMCNT,
		&ortin.nds.ppu->VRAMCNT_H,
		&ortin.nds.ppu->VRAMCNT_I,
		&ortin.nds.nds9->dsmath->DIVCNT,
		&ortin.nds.nds9->dsmath->SQRTCNT,
		&ortin.nds.nds9->dsmath->SQRT_RESULT,
		&ortin.nds.ppu->engineB.DISPCNT,
		&ortin.nds.ppu->engineB.bg[0].BGCNT,
		&ortin.nds.ppu->engineB.bg[1].BGCNT,
		&ortin.nds.ppu->engineB.bg[2].BGCNT,
		&ortin.nds.ppu->engineB.bg[3].BGCNT,
		&ortin.nds.ppu->engineB.bg[0].BGHOFS,
		&ortin.nds.ppu->engineB.bg[0].BGVOFS,
		&ortin.nds.ppu->engineB.bg[1].BGHOFS,
		&ortin.nds.ppu->engineB.bg[1].BGVOFS,
		&ortin.nds.ppu->engineB.bg[2].BGHOFS,
		&ortin.nds.ppu->engineB.bg[2].BGVOFS,
		&ortin.nds.ppu->engineB.bg[3].BGHOFS,
		&ortin.nds.ppu->engineB.bg[3].BGVOFS,
		&ortin.nds.ppu->engineB.MOSAIC,
		&ortin.nds.ppu->engineB.MASTER_BRIGHT,
	};

	static int selected = 0;
	static bool refresh = true;
	static bool tmpRefresh = false;

	ImGui::SetNextWindowSize(ImVec2(500, 440), ImGuiCond_FirstUseEver);
	ImGui::Begin("NDS9 IO Registers", &showIoReg9);

	// Left
	{
		ImGui::BeginChild("left pane", ImVec2(200, 0), true);
		for (int i = 0; i < registers9.size(); i++) {
			auto reg = registers9[i];
			if (ImGui::Selectable(fmt::format("{}: 0x{:0>7X} ({})", reg.name, reg.address, reg.readable == reg.writable ? "R/W" : (reg.readable ? "R" : "W")).c_str(), selected == i)) {
				if (i != selected)
					tmpRefresh = true;
				selected = i;
			}
		}
		ImGui::EndChild();
	}
	ImGui::SameLine();

	// Right
	int size = registers9[selected].size;
	u32 address = registers9[selected].address;
	static u32 value;
	{
		if (refresh || tmpRefresh) {
			if (size == 4) {
				value = *(u32 *)registerPointers9[selected];
			} else if (size == 2) {
				value = *(u16 *)registerPointers9[selected];
			} else {
				value = *(u8 *)registerPointers9[selected];
			}
		}

		ImGui::BeginGroup();
		ImGui::BeginChild("item view", ImVec2(0, -ImGui::GetFrameHeightWithSpacing())); // Leave room for 1 line below us

		ImGui::Text("%s", fmt::format("0x{:0>{}X} - {}", value, size * 2, registers9[selected].description).c_str());
		ImGui::Separator();

		for (int i = 0; i < registers9[selected].numFields; i++) {
			auto field = registers9[selected].fields[i];
			unsigned mask = ((1 << field.length) - 1) << field.startBit;
			u32 fValue = (value & mask) >> field.startBit;

			switch (field.type) {
			case TEXT_BOX:
				fValue = numberInput(((std::string)field.name).c_str(), false, fValue, mask >> field.startBit);
				break;
			case TEXT_BOX_HEX:
				fValue = numberInput(((std::string)field.name).c_str(), true, fValue, mask >> field.startBit);
				break;
			case CHECKBOX:
				ImGui::Checkbox(((std::string)field.name).c_str(), (bool *)&fValue);
				break;
			case COMBO:
				ImGui::Combo(((std::string)field.name).c_str(), (int *)&fValue, field.comboText);
				break;
			case SPECIAL:
				switch (address) {
				case 0x4000004: // DISPSTAT
					fValue = numberInput(((std::string)field.name).c_str(), false, (fValue >> 1) | ((fValue & 1) << 8), mask >> field.startBit);
					fValue = ((fValue & 0xFF) << 1) | (fValue >> 8);
					break;
				case 0x4000020: // BG2PA
				case 0x4000022: // BG2PB
				case 0x4000024: // BG2PC
				case 0x4000026: // BG2PD
				case 0x4000030: // BG3PA
				case 0x4000032: // BG3PB
				case 0x4000034: // BG3PC
				case 0x4000036: // BG3PD
					ImGui::Text("%f", (float)((i16)fValue) / 256);
					break;
				case 0x4000028: // BG2X
				case 0x400002C: // BG2Y
				case 0x4000038: // BG3X
				case 0x400003C: // BG3Y
					ImGui::Text("%f", (float)((i32)(fValue << 4) >> 4) / 256);
					break;
				}
				break;
			}

			value = (value & ~mask) | (fValue << field.startBit);
		}
		ImGui::EndChild();

		ImGui::Checkbox("Refresh", &refresh);
		ImGui::SameLine();

		tmpRefresh = false;
		if (ImGui::Button("Refresh"))
			tmpRefresh = true;
		ImGui::SameLine();

		if (ImGui::Button("Write")) {
			if (size == 4) {
				ortin.nds.nds9->writeIO(address | 0, (u8)(value >> 0), false);
				ortin.nds.nds9->writeIO(address | 1, (u8)(value >> 8), false);
				ortin.nds.nds9->writeIO(address | 2, (u8)(value >> 16), false);
				ortin.nds.nds9->writeIO(address | 3, (u8)(value >> 24), true);
			} else if (size == 2) {
				ortin.nds.nds9->writeIO(address | 0, (u8)(value >> 0), false);
				ortin.nds.nds9->writeIO(address | 1, (u8)(value >> 8), true);
			} else {
				ortin.nds.nds9->writeIO(address, (u8)value, true);
			}

			tmpRefresh = true;
		}
		ImGui::EndGroup();
	}

	ImGui::End();
}

static const std::array<IoRegister, 131> registers7 = {{
	{"DISPSTAT", "Display Status and Interrupt Control", 0x4000004, 2, true, true, 7, (IoField[]){
		{"V-Blank", 0, 1, CHECKBOX},
		{"H-Blank", 1, 1, CHECKBOX},
		{"VCOUNT Compare Status", 2, 1, CHECKBOX},
		{"V-Blank IRQ", 3, 1, CHECKBOX},
		{"H-Blank IRQ", 4, 1, CHECKBOX},
		{"VCOUNT Compare IRQ", 5, 1, CHECKBOX},
		{"VCOUNT Compare Value", 7, 9, SPECIAL}}},
	{"VCOUNT", "Vertical Counter", 0x4000006, 2, true, false, 1, (IoField[]){
		{"Current Scanline (LY)", 0, 9}}},
	{"DMA0SAD", "DMA 0 Source Address", 0x40000B0, 4, true, true, 1, (IoField[]){
		{"DMA 0 Source Address", 0, 27, TEXT_BOX_HEX}}},
	{"DMA0DAD", "DMA 0 Destination Address", 0x40000B4, 4, true, true, 1, (IoField[]){
		{"DMA 0 Destination Address", 0, 27, TEXT_BOX_HEX}}},
	{"DMA0CNT", "DMA 0 Control", 0x40000B8, 4, true, true, 8, (IoField[]){
		{"Word Count", 0, 14, TEXT_BOX_HEX},
		{"Dest Addr Control", 21, 2, COMBO, "Increment\0"
											"Decrement\0"
											"Fixed\0"
											"Increment/Reload\0\0"},
		{"Source Adr Control", 23, 2, COMBO, "Increment\0"
											 "Decrement\0"
											 "Fixed\0"
											 "Prohibited\0\0"},
		{"DMA Repeat", 25, 1, CHECKBOX},
		{"DMA Transfer Type", 26, 1, CHECKBOX},
		{"DMA Start Timing", 28, 2, COMBO, "Start Immediately\0"
										   "Start at V-Blank\0"
										   "DS Cartridge Slot\0"
										   "Wireless interrupt\0\0"},
		{"IRQ upon end of Word Count", 30, 1, CHECKBOX},
		{"DMA Enable", 31, 1, CHECKBOX}}},
	{"DMA1SAD", "DMA 1 Source Address", 0x40000BC, 4, true, true, 1, (IoField[]){
		{"DMA 0 Source Address", 0, 28, TEXT_BOX_HEX}}},
	{"DMA1DAD", "DMA 1 Destination Address", 0x40000C0, 4, true, true, 1, (IoField[]){
		{"DMA 0 Destination Address", 0, 27, TEXT_BOX_HEX}}},
	{"DMA1CNT", "DMA 1 Control", 0x40000C4, 4, true, true, 8, (IoField[]){
		{"Word Count", 0, 14, TEXT_BOX_HEX},
		{"Dest Addr Control", 21, 2, COMBO, "Increment\0"
											"Decrement\0"
											"Fixed\0"
											"Increment/Reload\0\0"},
		{"Source Adr Control", 23, 2, COMBO, "Increment\0"
											 "Decrement\0"
											 "Fixed\0"
											 "Prohibited\0\0"},
		{"DMA Repeat", 25, 1, CHECKBOX},
		{"DMA Transfer Type", 26, 1, CHECKBOX},
		{"DMA Start Timing", 28, 2, COMBO, "Start Immediately\0"
										   "Start at V-Blank\0"
										   "DS Cartridge Slot\0"
										   "GBA Cartridge Slot\0\0"},
		{"IRQ upon end of Word Count", 30, 1, CHECKBOX},
		{"DMA Enable", 31, 1, CHECKBOX}}},
	{"DMA2SAD", "DMA 2 Source Address", 0x40000C8, 4, true, true, 1, (IoField[]){
		{"DMA 0 Source Address", 0, 28, TEXT_BOX_HEX}}},
	{"DMA2DAD", "DMA 2 Destination Address", 0x40000CC, 4, true, true, 1, (IoField[]){
		{"DMA 0 Destination Address", 0, 27, TEXT_BOX_HEX}}},
	{"DMA2CNT", "DMA 2 Control", 0x40000D0, 4, true, true, 8, (IoField[]){
		{"Word Count", 0, 14, TEXT_BOX_HEX},
		{"Dest Addr Control", 21, 2, COMBO, "Increment\0"
											"Decrement\0"
											"Fixed\0"
											"Increment/Reload\0\0"},
		{"Source Adr Control", 23, 2, COMBO, "Increment\0"
											 "Decrement\0"
											 "Fixed\0"
											 "Prohibited\0\0"},
		{"DMA Repeat", 25, 1, CHECKBOX},
		{"DMA Transfer Type", 26, 1, CHECKBOX},
		{"DMA Start Timing", 28, 2, COMBO, "Start Immediately\0"
										   "Start at V-Blank\0"
										   "DS Cartridge Slot\0"
										   "Wireless interrupt\0\0"},
		{"IRQ upon end of Word Count", 30, 1, CHECKBOX},
		{"DMA Enable", 31, 1, CHECKBOX}}},
	{"DMA3SAD", "DMA 3 Source Address", 0x40000D4, 4, true, true, 1, (IoField[]){
		{"DMA 0 Source Address", 0, 28, TEXT_BOX_HEX}}},
	{"DMA3DAD", "DMA 3 Destination Address", 0x40000D8, 4, true, true, 1, (IoField[]){
		{"DMA 0 Destination Address", 0, 28, TEXT_BOX_HEX}}},
	{"DMA3CNT", "DMA 3 Control", 0x40000DC, 4, true, true, 8, (IoField[]){
		{"Word Count", 0, 16, TEXT_BOX_HEX},
		{"Dest Addr Control", 21, 2, COMBO, "Increment\0"
											"Decrement\0"
											"Fixed\0"
											"Increment/Reload\0\0"},
		{"Source Adr Control", 23, 2, COMBO, "Increment\0"
											 "Decrement\0"
											 "Fixed\0"
											 "Prohibited\0\0"},
		{"DMA Repeat", 25, 1, CHECKBOX},
		{"DMA Transfer Type", 26, 1, CHECKBOX},
		{"DMA Start Timing", 28, 2, COMBO, "Start Immediately\0"
										   "Start at V-Blank\0"
										   "DS Cartridge Slot\0"
										   "GBA Cartridge Slot\0\0"},
		{"IRQ upon end of Word Count", 30, 1, CHECKBOX},
		{"DMA Enable", 31, 1, CHECKBOX}}},
	{"TM0CNT_L", "Timer 0 Counter/Reload", 0x4000100, 2, true, true, 1, (IoField[]){
		{"Counter(R)/Reload(W)", 0, 16, TEXT_BOX_HEX}}},
	{"TM0CNT_H", "Timer 0 Control", 0x4000102, 2, true, true, 3, (IoField[]){
		{"Prescaler Selection", 0, 2, COMBO, "F/1\0"
											 "F/64\0"
											 "F/256\0"
											 "F/1024\0\0"},
		{"Timer IRQ Enable", 6, 1, CHECKBOX},
		{"Timer Start/Stop", 7, 1, CHECKBOX}}},
	{"TM1CNT_L", "Timer 1 Counter/Reload", 0x4000104, 2, true, true, 1, (IoField[]){
		{"Counter(R)/Reload(W)", 0, 16, TEXT_BOX_HEX}}},
	{"TM1CNT_H", "Timer 1 Control", 0x4000106, 2, true, true, 4, (IoField[]){
		{"Prescaler Selection", 0, 2, COMBO, "F/1\0"
											 "F/64\0"
											 "F/256\0"
											 "F/1024\0\0"},
		{"Count-up Timing", 2, 1, CHECKBOX},
		{"Timer IRQ Enable", 6, 1, CHECKBOX},
		{"Timer Start/Stop", 7, 1, CHECKBOX}}},
	{"TM2CNT_L", "Timer 2 Counter/Reload", 0x4000108, 2, true, true, 1, (IoField[]){
		{"Counter(R)/Reload(W)", 0, 16, TEXT_BOX_HEX}}},
	{"TM2CNT_H", "Timer 2 Control", 0x400010A, 2, true, true, 4, (IoField[]){
		{"Prescaler Selection", 0, 2, COMBO, "F/1\0"
											 "F/64\0"
											 "F/256\0"
											 "F/1024\0\0"},
		{"Count-up Timing", 2, 1, CHECKBOX},
		{"Timer IRQ Enable", 6, 1, CHECKBOX},
		{"Timer Start/Stop", 7, 1, CHECKBOX}}},
	{"TM3CNT_L", "Timer 3 Counter/Reload", 0x400010C, 2, true, true, 1, (IoField[]){
		{"Counter(R)/Reload(W)", 0, 16, TEXT_BOX_HEX}}},
	{"TM3CNT_H", "Timer 3 Control", 0x400010E, 2, true, true, 4, (IoField[]){
		{"Prescaler Selection", 0, 2, COMBO, "F/1\0"
											 "F/64\0"
											 "F/256\0"
											 "F/1024\0\0"},
		{"Count-up Timing", 2, 1, CHECKBOX},
		{"Timer IRQ Enable", 6, 1, CHECKBOX},
		{"Timer Start/Stop", 7, 1, CHECKBOX}}},
	{"KEYINPUT", "Key Status (Inverted)", 0x4000130, 2, true, false, 10, (IoField[]){
		{"A", 0, 1, CHECKBOX},
		{"B", 1, 1, CHECKBOX},
		{"Select", 2, 1, CHECKBOX},
		{"Start", 3, 1, CHECKBOX},
		{"Right", 4, 1, CHECKBOX},
		{"Left", 5, 1, CHECKBOX},
		{"Up", 6, 1, CHECKBOX},
		{"Down", 7, 1, CHECKBOX},
		{"R", 8, 1, CHECKBOX},
		{"L", 9, 1, CHECKBOX}}},
	{"KEYCNT", "Key Interrupt Control", 0x4000132, 2, true, true, 12, (IoField[]){
		{"A", 0, 1, CHECKBOX},
		{"B", 1, 1, CHECKBOX},
		{"Select", 2, 1, CHECKBOX},
		{"Start", 3, 1, CHECKBOX},
		{"Right", 4, 1, CHECKBOX},
		{"Left", 5, 1, CHECKBOX},
		{"Up", 6, 1, CHECKBOX},
		{"Down", 7, 1, CHECKBOX},
		{"R", 8, 1, CHECKBOX},
		{"L", 9, 1, CHECKBOX},
		{"IRQ Enable", 14, 1, CHECKBOX},
		{"Condition", 15, 1, COMBO, "OR Mode\0"
									"AND Mode\0\0"}}},
	{"EXTKEYIN", "Key X/Y Input (Inverted)", 0x4000136, 2, true, false, 5, (IoField[]){
		{"X", 0, 1, CHECKBOX},
		{"Y", 1, 1, CHECKBOX},
		{"DEBUG button", 3, 1, CHECKBOX},
		{"Pen down", 6, 1, CHECKBOX},
		{"Hinge open", 7, 1, CHECKBOX}}},
	{"RTC Realtime Clock Bus", "Real Time Clock Register", 0x4000138, 1, true, true, 6, (IoField[]){
		{"Data I/O", 0, 1, CHECKBOX},
		{"Clock Out", 1, 1, CHECKBOX},
		{"Select Out", 2, 1, CHECKBOX},
		{"Data Direction", 4, 1, CHECKBOX},
		{"Clock Direction", 5, 1, CHECKBOX},
		{"Select Direction", 6, 1, CHECKBOX}}},
	{"IPCSYNC", "IPC Synchronize Register", 0x4000180, 2, true, true, 4, (IoField[]){
		{"Data input from IPCSYNC of remote CPU", 0, 4, TEXT_BOX_HEX},
		{"Data output to IPCSYNC of remote CPU", 8, 4, TEXT_BOX_HEX},
		{"Send IRQ to remote CPU", 13, 1, CHECKBOX},
		{"Enable IRQ from remote CPU", 14, 1, CHECKBOX}}},
	{"IPCFIFOCNT", "IPC Fifo Control Register", 0x4000184, 2, true, true, 9, (IoField[]){
		{"Send Fifo Empty Status", 0, 1, CHECKBOX},
		{"Send Fifo Full Status", 1, 1, CHECKBOX},
		{"Send Fifo Empty IRQ", 2, 1, CHECKBOX},
		{"Send Fifo Clear", 3, 1, CHECKBOX},
		{"Receive Fifo Empty Status", 8, 1, CHECKBOX},
		{"Receive Fifo Full Status", 9, 1, CHECKBOX},
		{"Receive Fifo Not Empty IRQ", 10, 1, CHECKBOX},
		{"Error, Read Empty/Send Full", 14, 1, CHECKBOX},
		{"Enable Send/Receive Fifo", 15, 1, CHECKBOX}}},
	{"AUXSPICNT", "Gamecard ROM and SPI Control", 0x40001A0, 2, true, true, 6, (IoField[]){
		{"SPI Baudrate", 0, 2, COMBO, "4MHz/Default\0"
									  "2MHz\0"
									  "1MHz\0"
									  "512KHz\0\0"},
		{"SPI Hold Chipselect", 6, 1, CHECKBOX},
		{"SPI Busy", 7, 1, CHECKBOX},
		{"NDS Slot Mode", 13, 1, COMBO, "Parallel/ROM\0"
										"Serial/SPI-Backup\0\0"},
		{"Transfer Ready IRQ", 14, 1, CHECKBOX},
		{"NDS Slot Enable", 15, 1, CHECKBOX}}},
	{"ROMCTRL", "Gamecard Bus ROMCTRL", 0x40001A4, 4, true, true, 12, (IoField[]){
		{"KEY1 gap1 length", 0, 13, TEXT_BOX_HEX},
		{"KEY2 encrypt data", 13, 1, CHECKBOX},
		{"KEY2 Apply Seed", 15, 1, CHECKBOX},
		{"KEY1 gap2 length", 16, 6, TEXT_BOX_HEX},
		{"KEY2 encrypt cmd", 22, 1, CHECKBOX},
		{"Data-Word Status", 23, 1, CHECKBOX},
		{"Data Block size", 24, 3, COMBO, "None\0"
										  "200h bytes\0"
										  "400h bytes\0"
										  "800h bytes\0"
										  "1000h bytes\0"
										  "2000h bytes\0"
										  "4000h bytes\0"
										  "4 bytes\0\0"},
		{"Transfer CLK rate", 27, 1, COMBO, "6.7MHz=33.51MHz/5\0"
											"4.2MHz=33.51MHz/8\0\0"},
		{"KEY1 Gap CLKs", 28, 1, CHECKBOX},
		{"RESB Release Reset", 29, 1, CHECKBOX},
		{"Data Direction \"WR\"", 30, 1, CHECKBOX},
		{"Block Start/Status", 31, 1, CHECKBOX}}},
	{"Encryption Seed 0 Lower 32bit", "Encryption Seed 0 Lower 32bit", 0x40001B0, 4, false, true, 1, (IoField[]){
		{"Encryption Seed 0 Lower 32bit", 0, 32, TEXT_BOX_HEX}}},
	{"Encryption Seed 1 Lower 32bit", "Encryption Seed 1 Lower 32bit", 0x40001B4, 4, false, true, 1, (IoField[]){
		{"Encryption Seed 1 Lower 32bit", 0, 32, TEXT_BOX_HEX}}},
	{"Encryption Seed 0 Upper 7bit", "Encryption Seed 0 Upper 7bit", 0x40001B8, 2, false, true, 1, (IoField[]){
		{"Encryption Seed 0 Upper 7bit", 0, 7, TEXT_BOX_HEX}}},
	{"Encryption Seed 1 Upper 7bit", "Encryption Seed 1 Upper 7bit", 0x40001BA, 2, false, true, 1, (IoField[]){
		{"Encryption Seed 1 Upper 7bit", 0, 7, TEXT_BOX_HEX}}},
	{"SPICNT", "SPI Bus Control/Status Register", 0x40001C0, 2, true, true, 7, (IoField[]){
		{"Baudrate", 0, 2, COMBO, "4MHz/Firmware\0"
								  "2MHz/Touchscr\0"
								  "1MHz/Powerman.\0"
								  "512KHz\0\0"},
		{"Busy Flag", 7, 1, CHECKBOX},
		{"Device Select", 8, 2, COMBO, "Powerman.\0"
									   "Firmware\0"
									   "Touchscr\0"
									   "Reserved\0\0"},
		{"Transfer Size", 10, 1, COMBO, "8bit/Normal\0"
										"16bit/Bugged\0\0"},
		{"Chipselect Hold", 11, 1, CHECKBOX},
		{"Interrupt Request", 14, 1, CHECKBOX},
		{"SPI Bus Enable", 15, 1, CHECKBOX}}},
	{"SPIDATA", "SPI Bus Data/Strobe Register", 0x40001C2, 2, true, true, 1, (IoField[]){
		{"Data", 0, 8, TEXT_BOX_HEX}}},
	{"EXMEMSTAT","External Memory Status", 0x4000204, 2, true, true, 8, (IoField[]){
		{"32-pin GBA Slot SRAM Access Time", 0, 2, COMBO, "10 cycles\0"
														  "8 cycles\0"
														  "6 cycles\0"
														  "18 cycles\0\0"},
		{"32-pin GBA Slot ROM 1st Access Time", 2, 2, COMBO, "10 cycles\0"
															 "8 cycles\0"
															 "6 cycles\0"
															 "18 cycles\0\0"},
		{"32-pin GBA Slot ROM 2nd Access Time", 4, 1, COMBO, "6 cycles\0"
															 "4 cycles\0\0"},
		{"32-pin GBA Slot PHI-pin out", 5, 2, COMBO, "Low\0"
													 "4.19MHz\0"
													 "8.38MHz\0"
													 "16.76MHz\0\0"},
		{"32-pin GBA Slot Access Rights", 7, 1, COMBO, "ARM9\0"
													   "ARM7\0\0"},
		{"17-pin NDS Slot Access Rights", 11, 1, COMBO, "ARM9\0"
														"ARM7\0\0"},
		{"Main Memory Interface Mode Switch", 14, 1, COMBO, "Async/GBA/Reserved\0"
															"Synchronous\0\0"},
		{"Main Memory Access Priority", 15, 1, COMBO, "ARM9 Priority\0"
													  "ARM7 Priority\0\0"}}},
	{"IME", "Interrupt Master Enable", 0x4000208, 4, true, true, 1, (IoField[]){
		{"Enable Interrupts", 0, 1, CHECKBOX}}},
	{"IE", "Interrupt Enable", 0x4000210, 4, true, true, 22, (IoField[]){
		{"LCD V-BLank", 0,  1, CHECKBOX},
		{"LCD H-BLank", 1,  1, CHECKBOX},
		{"LCD V-Counter Match", 2,  1, CHECKBOX},
		{"Timer 0 Overflow", 3,  1, CHECKBOX},
		{"Timer 1 Overflow", 4,  1, CHECKBOX},
		{"Timer 2 Overflow", 5,  1, CHECKBOX},
		{"Timer 3 Overflow", 6,  1, CHECKBOX},
		{"SIO/RCNT/RTC", 7,  1, CHECKBOX},
		{"DMA 0", 8,  1, CHECKBOX},
		{"DMA 1", 9,  1, CHECKBOX},
		{"DMA 2", 10, 1, CHECKBOX},
		{"DMA 3", 11, 1, CHECKBOX},
		{"Keypad", 12, 1, CHECKBOX},
		{"GBA Slot", 13, 1, CHECKBOX},
		{"IPC Sync",  16, 1, CHECKBOX},
		{"IPC Send FIFO Empty", 17, 1, CHECKBOX},
		{"IPC Recv FIFO Not Empty", 18, 1, CHECKBOX},
		{"NDS-Slot Game Card Data Transfer Completion", 19, 1, CHECKBOX},
		{"NDS-Slot Game Card IREQ_MC", 20, 1, CHECKBOX},
		{"Screens unfolding", 22, 1, CHECKBOX},
		{"SPI bus", 23, 1, CHECKBOX},
		{"Wifi", 24, 1, CHECKBOX}}},
	{"IF", "Interrupt Request Flags", 0x4000214, 4, true, true, 22, (IoField[]){
		{"LCD V-BLank", 0,  1, CHECKBOX},
		{"LCD H-BLank", 1,  1, CHECKBOX},
		{"LCD V-Counter Match", 2,  1, CHECKBOX},
		{"Timer 0 Overflow", 3,  1, CHECKBOX},
		{"Timer 1 Overflow", 4,  1, CHECKBOX},
		{"Timer 2 Overflow", 5,  1, CHECKBOX},
		{"Timer 3 Overflow", 6,  1, CHECKBOX},
		{"SIO/RCNT/RTC", 7,  1, CHECKBOX},
		{"DMA 0", 8,  1, CHECKBOX},
		{"DMA 1", 9,  1, CHECKBOX},
		{"DMA 2", 10, 1, CHECKBOX},
		{"DMA 3", 11, 1, CHECKBOX},
		{"Keypad", 12, 1, CHECKBOX},
		{"GBA Slot", 13, 1, CHECKBOX},
		{"IPC Sync",  16, 1, CHECKBOX},
		{"IPC Send FIFO Empty", 17, 1, CHECKBOX},
		{"IPC Recv FIFO Not Empty", 18, 1, CHECKBOX},
		{"NDS-Slot Game Card Data Transfer Completion", 19, 1, CHECKBOX},
		{"NDS-Slot Game Card IREQ_MC", 20, 1, CHECKBOX},
		{"Screens unfolding", 22, 1, CHECKBOX},
		{"SPI bus", 23, 1, CHECKBOX},
		{"Wifi", 24, 1, CHECKBOX}}},
	{"VRAMSTAT", "WRAM Bank Status", 0x4000240, 1, true, false, 2, (IoField[]){
		{"VRAM C enabled and allocated to NDS7", 0, 1, CHECKBOX},
		{"VRAM D enabled and allocated to NDS7", 1, 1, CHECKBOX}}},
	{"WRAMSTAT", "WRAM Bank Status", 0x4000241, 1, true, false, 1, (IoField[]){
		{"Mapping", 0, 2, COMBO, "Unmapped/WRAM Mirror\0"
								 "Bottom 16KB\0"
								 "Top 16KB\0"
								 "Full 32KB\0\0"}}},
	{"HALTCNT", "Low Power Mode Control", 0x4000301, 1, true, true, 1, (IoField[]){
		{"Power Down Mode", 6, 2, COMBO, "No function\0"
										 "Enter GBA Mode\0"
										 "Halt\0"
										 "Sleep\0\0"}}},
	{"SOUND0CNT", "Sound Channel 0 Control Register", 0x4000400, 4, true, true, 8, (IoField[]){
		{"Volume Mul", 0, 7, TEXT_BOX},
		{"Volume Div", 8, 2, COMBO, "Normal\0"
									"Div2\0"
									"Div4\0"
									"Div16\0\0"},
		{"Hold", 15, 1, CHECKBOX},
		{"Panning", 16, 7, TEXT_BOX},
		{"Wave Duty", 24, 3, TEXT_BOX},
		{"Repeat Mode", 27, 2, COMBO, "Manual\0"
									  "Loop Infinite\0"
									  "One-Shot\0"
									  "Prohibited\0\0"},
		{"Format", 29, 2, COMBO, "PCM8\0"
								 "PCM16\0"
								 "IMA-ADPCM\0"
								 "PSG/Noise\0\0"},
		{"Start/Status", 31, 1, CHECKBOX}}},
	{"SOUND0SAD", "Sound Channel 0 Data Source Register", 0x4000404, 4, false, true, 1, (IoField[]){
		{"Source Address", 0, 27, TEXT_BOX_HEX}}},
	{"SOUND0TMR", "Sound Channel 0 Timer Register", 0x4000408, 2, false, true, 1, (IoField[]){
		{"Timer Value, Sample frequency", 0, 16, TEXT_BOX_HEX}}},
	{"SOUND0PNT", "Sound Channel 0 Loopstart Register", 0x400040A, 2, false, true, 1, (IoField[]){
		{"Loop Start, Sample loop start position", 0, 16, TEXT_BOX_HEX}}},
	{"SOUND0LEN", "Sound Channel 0 Length Register", 0x400040C, 4, false, true, 1, (IoField[]){
		{"Sound length", 0, 22, TEXT_BOX_HEX}}},
	{"SOUND1CNT", "Sound Channel 1 Control Register", 0x4000410, 4, true, true, 8, (IoField[]){
		{"Volume Mul", 0, 7, TEXT_BOX},
		{"Volume Div", 8, 2, COMBO, "Normal\0"
									"Div2\0"
									"Div4\0"
									"Div16\0\0"},
		{"Hold", 15, 1, CHECKBOX},
		{"Panning", 16, 7, TEXT_BOX},
		{"Wave Duty", 24, 3, TEXT_BOX},
		{"Repeat Mode", 27, 2, COMBO, "Manual\0"
									  "Loop Infinite\0"
									  "One-Shot\0"
									  "Prohibited\0\0"},
		{"Format", 29, 2, COMBO, "PCM8\0"
								 "PCM16\0"
								 "IMA-ADPCM\0"
								 "PSG/Noise\0\0"},
		{"Start/Status", 31, 1, CHECKBOX}}},
	{"SOUND1SAD", "Sound Channel 1 Data Source Register", 0x4000414, 4, false, true, 1, (IoField[]){
		{"Source Address", 0, 27, TEXT_BOX_HEX}}},
	{"SOUND1TMR", "Sound Channel 1 Timer Register", 0x4000418, 2, false, true, 1, (IoField[]){
		{"Timer Value, Sample frequency", 0, 16, TEXT_BOX_HEX}}},
	{"SOUND1PNT", "Sound Channel 1 Loopstart Register", 0x400041A, 2, false, true, 1, (IoField[]){
		{"Loop Start, Sample loop start position", 0, 16, TEXT_BOX_HEX}}},
	{"SOUND1LEN", "Sound Channel 1 Length Register", 0x400041C, 4, false, true, 1, (IoField[]){
		{"Sound length", 0, 22, TEXT_BOX_HEX}}},
	{"SOUND2CNT", "Sound Channel 2 Control Register", 0x4000420, 4, true, true, 8, (IoField[]){
		{"Volume Mul", 0, 7, TEXT_BOX},
		{"Volume Div", 8, 2, COMBO, "Normal\0"
									"Div2\0"
									"Div4\0"
									"Div16\0\0"},
		{"Hold", 15, 1, CHECKBOX},
		{"Panning", 16, 7, TEXT_BOX},
		{"Wave Duty", 24, 3, TEXT_BOX},
		{"Repeat Mode", 27, 2, COMBO, "Manual\0"
									  "Loop Infinite\0"
									  "One-Shot\0"
									  "Prohibited\0\0"},
		{"Format", 29, 2, COMBO, "PCM8\0"
								 "PCM16\0"
								 "IMA-ADPCM\0"
								 "PSG/Noise\0\0"},
		{"Start/Status", 31, 1, CHECKBOX}}},
	{"SOUND2SAD", "Sound Channel 2 Data Source Register", 0x4000424, 4, false, true, 1, (IoField[]){
		{"Source Address", 0, 27, TEXT_BOX_HEX}}},
	{"SOUND2TMR", "Sound Channel 2 Timer Register", 0x4000428, 2, false, true, 1, (IoField[]){
		{"Timer Value, Sample frequency", 0, 16, TEXT_BOX_HEX}}},
	{"SOUND2PNT", "Sound Channel 2 Loopstart Register", 0x400042A, 2, false, true, 1, (IoField[]){
		{"Loop Start, Sample loop start position", 0, 16, TEXT_BOX_HEX}}},
	{"SOUND2LEN", "Sound Channel 2 Length Register", 0x400042C, 4, false, true, 1, (IoField[]){
		{"Sound length", 0, 22, TEXT_BOX_HEX}}},
	{"SOUND3CNT", "Sound Channel 3 Control Register", 0x4000430, 4, true, true, 8, (IoField[]){
		{"Volume Mul", 0, 7, TEXT_BOX},
		{"Volume Div", 8, 2, COMBO, "Normal\0"
									"Div2\0"
									"Div4\0"
									"Div16\0\0"},
		{"Hold", 15, 1, CHECKBOX},
		{"Panning", 16, 7, TEXT_BOX},
		{"Wave Duty", 24, 3, TEXT_BOX},
		{"Repeat Mode", 27, 2, COMBO, "Manual\0"
									  "Loop Infinite\0"
									  "One-Shot\0"
									  "Prohibited\0\0"},
		{"Format", 29, 2, COMBO, "PCM8\0"
								 "PCM16\0"
								 "IMA-ADPCM\0"
								 "PSG/Noise\0\0"},
		{"Start/Status", 31, 1, CHECKBOX}}},
	{"SOUND3SAD", "Sound Channel 3 Data Source Register", 0x4000434, 4, false, true, 1, (IoField[]){
		{"Source Address", 0, 27, TEXT_BOX_HEX}}},
	{"SOUND3TMR", "Sound Channel 3 Timer Register", 0x4000438, 2, false, true, 1, (IoField[]){
		{"Timer Value, Sample frequency", 0, 16, TEXT_BOX_HEX}}},
	{"SOUND3PNT", "Sound Channel 3 Loopstart Register", 0x400043A, 2, false, true, 1, (IoField[]){
		{"Loop Start, Sample loop start position", 0, 16, TEXT_BOX_HEX}}},
	{"SOUND3LEN", "Sound Channel 3 Length Register", 0x400043C, 4, false, true, 1, (IoField[]){
		{"Sound length", 0, 22, TEXT_BOX_HEX}}},
	{"SOUND4CNT", "Sound Channel 4 Control Register", 0x4000440, 4, true, true, 8, (IoField[]){
		{"Volume Mul", 0, 7, TEXT_BOX},
		{"Volume Div", 8, 2, COMBO, "Normal\0"
									"Div2\0"
									"Div4\0"
									"Div16\0\0"},
		{"Hold", 15, 1, CHECKBOX},
		{"Panning", 16, 7, TEXT_BOX},
		{"Wave Duty", 24, 3, TEXT_BOX},
		{"Repeat Mode", 27, 2, COMBO, "Manual\0"
									  "Loop Infinite\0"
									  "One-Shot\0"
									  "Prohibited\0\0"},
		{"Format", 29, 2, COMBO, "PCM8\0"
								 "PCM16\0"
								 "IMA-ADPCM\0"
								 "PSG/Noise\0\0"},
		{"Start/Status", 31, 1, CHECKBOX}}},
	{"SOUND4SAD", "Sound Channel 4 Data Source Register", 0x4000444, 4, false, true, 1, (IoField[]){
		{"Source Address", 0, 27, TEXT_BOX_HEX}}},
	{"SOUND4TMR", "Sound Channel 4 Timer Register", 0x4000448, 2, false, true, 1, (IoField[]){
		{"Timer Value, Sample frequency", 0, 16, TEXT_BOX_HEX}}},
	{"SOUND4PNT", "Sound Channel 4 Loopstart Register", 0x400044A, 2, false, true, 1, (IoField[]){
		{"Loop Start, Sample loop start position", 0, 16, TEXT_BOX_HEX}}},
	{"SOUND4LEN", "Sound Channel 4 Length Register", 0x400044C, 4, false, true, 1, (IoField[]){
		{"Sound length", 0, 22, TEXT_BOX_HEX}}},
	{"SOUND5CNT", "Sound Channel 5 Control Register", 0x4000450, 4, true, true, 8, (IoField[]){
		{"Volume Mul", 0, 7, TEXT_BOX},
		{"Volume Div", 8, 2, COMBO, "Normal\0"
									"Div2\0"
									"Div4\0"
									"Div16\0\0"},
		{"Hold", 15, 1, CHECKBOX},
		{"Panning", 16, 7, TEXT_BOX},
		{"Wave Duty", 24, 3, TEXT_BOX},
		{"Repeat Mode", 27, 2, COMBO, "Manual\0"
									  "Loop Infinite\0"
									  "One-Shot\0"
									  "Prohibited\0\0"},
		{"Format", 29, 2, COMBO, "PCM8\0"
								 "PCM16\0"
								 "IMA-ADPCM\0"
								 "PSG/Noise\0\0"},
		{"Start/Status", 31, 1, CHECKBOX}}},
	{"SOUND5SAD", "Sound Channel 5 Data Source Register", 0x4000454, 4, false, true, 1, (IoField[]){
		{"Source Address", 0, 27, TEXT_BOX_HEX}}},
	{"SOUND5TMR", "Sound Channel 5 Timer Register", 0x4000458, 2, false, true, 1, (IoField[]){
		{"Timer Value, Sample frequency", 0, 16, TEXT_BOX_HEX}}},
	{"SOUND5PNT", "Sound Channel 5 Loopstart Register", 0x400045A, 2, false, true, 1, (IoField[]){
		{"Loop Start, Sample loop start position", 0, 16, TEXT_BOX_HEX}}},
	{"SOUND5LEN", "Sound Channel 5 Length Register", 0x400045C, 4, false, true, 1, (IoField[]){
		{"Sound length", 0, 22, TEXT_BOX_HEX}}},
	{"SOUND6CNT", "Sound Channel 6 Control Register", 0x4000460, 4, true, true, 8, (IoField[]){
		{"Volume Mul", 0, 7, TEXT_BOX},
		{"Volume Div", 8, 2, COMBO, "Normal\0"
									"Div2\0"
									"Div4\0"
									"Div16\0\0"},
		{"Hold", 15, 1, CHECKBOX},
		{"Panning", 16, 7, TEXT_BOX},
		{"Wave Duty", 24, 3, TEXT_BOX},
		{"Repeat Mode", 27, 2, COMBO, "Manual\0"
									  "Loop Infinite\0"
									  "One-Shot\0"
									  "Prohibited\0\0"},
		{"Format", 29, 2, COMBO, "PCM8\0"
								 "PCM16\0"
								 "IMA-ADPCM\0"
								 "PSG/Noise\0\0"},
		{"Start/Status", 31, 1, CHECKBOX}}},
	{"SOUND6SAD", "Sound Channel 6 Data Source Register", 0x4000464, 4, false, true, 1, (IoField[]){
		{"Source Address", 0, 27, TEXT_BOX_HEX}}},
	{"SOUND6TMR", "Sound Channel 6 Timer Register", 0x4000468, 2, false, true, 1, (IoField[]){
		{"Timer Value, Sample frequency", 0, 16, TEXT_BOX_HEX}}},
	{"SOUND6PNT", "Sound Channel 6 Loopstart Register", 0x400046A, 2, false, true, 1, (IoField[]){
		{"Loop Start, Sample loop start position", 0, 16, TEXT_BOX_HEX}}},
	{"SOUND6LEN", "Sound Channel 6 Length Register", 0x400046C, 4, false, true, 1, (IoField[]){
		{"Sound length", 0, 22, TEXT_BOX_HEX}}},
	{"SOUND7CNT", "Sound Channel 7 Control Register", 0x4000470, 4, true, true, 8, (IoField[]){
		{"Volume Mul", 0, 7, TEXT_BOX},
		{"Volume Div", 8, 2, COMBO, "Normal\0"
									"Div2\0"
									"Div4\0"
									"Div16\0\0"},
		{"Hold", 15, 1, CHECKBOX},
		{"Panning", 16, 7, TEXT_BOX},
		{"Wave Duty", 24, 3, TEXT_BOX},
		{"Repeat Mode", 27, 2, COMBO, "Manual\0"
									  "Loop Infinite\0"
									  "One-Shot\0"
									  "Prohibited\0\0"},
		{"Format", 29, 2, COMBO, "PCM8\0"
								 "PCM16\0"
								 "IMA-ADPCM\0"
								 "PSG/Noise\0\0"},
		{"Start/Status", 31, 1, CHECKBOX}}},
	{"SOUND7SAD", "Sound Channel 7 Data Source Register", 0x4000474, 4, false, true, 1, (IoField[]){
		{"Source Address", 0, 27, TEXT_BOX_HEX}}},
	{"SOUND7TMR", "Sound Channel 7 Timer Register", 0x4000478, 2, false, true, 1, (IoField[]){
		{"Timer Value, Sample frequency", 0, 16, TEXT_BOX_HEX}}},
	{"SOUND7PNT", "Sound Channel 7 Loopstart Register", 0x400047A, 2, false, true, 1, (IoField[]){
		{"Loop Start, Sample loop start position", 0, 16, TEXT_BOX_HEX}}},
	{"SOUND7LEN", "Sound Channel 7 Length Register", 0x400047C, 4, false, true, 1, (IoField[]){
		{"Sound length", 0, 22, TEXT_BOX_HEX}}},
	{"SOUND8CNT", "Sound Channel 8 Control Register", 0x4000480, 4, true, true, 8, (IoField[]){
		{"Volume Mul", 0, 7, TEXT_BOX},
		{"Volume Div", 8, 2, COMBO, "Normal\0"
									"Div2\0"
									"Div4\0"
									"Div16\0\0"},
		{"Hold", 15, 1, CHECKBOX},
		{"Panning", 16, 7, TEXT_BOX},
		{"Wave Duty", 24, 3, TEXT_BOX},
		{"Repeat Mode", 27, 2, COMBO, "Manual\0"
									  "Loop Infinite\0"
									  "One-Shot\0"
									  "Prohibited\0\0"},
		{"Format", 29, 2, COMBO, "PCM8\0"
								 "PCM16\0"
								 "IMA-ADPCM\0"
								 "PSG/Noise\0\0"},
		{"Start/Status", 31, 1, CHECKBOX}}},
	{"SOUND8SAD", "Sound Channel 8 Data Source Register", 0x4000484, 4, false, true, 1, (IoField[]){
		{"Source Address", 0, 27, TEXT_BOX_HEX}}},
	{"SOUND8TMR", "Sound Channel 8 Timer Register", 0x4000488, 2, false, true, 1, (IoField[]){
		{"Timer Value, Sample frequency", 0, 16, TEXT_BOX_HEX}}},
	{"SOUND8PNT", "Sound Channel 8 Loopstart Register", 0x400048A, 2, false, true, 1, (IoField[]){
		{"Loop Start, Sample loop start position", 0, 16, TEXT_BOX_HEX}}},
	{"SOUND8LEN", "Sound Channel 8 Length Register", 0x400048C, 4, false, true, 1, (IoField[]){
		{"Sound length", 0, 22, TEXT_BOX_HEX}}},
	{"SOUND9CNT", "Sound Channel 9 Control Register", 0x4000490, 4, true, true, 8, (IoField[]){
		{"Volume Mul", 0, 7, TEXT_BOX},
		{"Volume Div", 8, 2, COMBO, "Normal\0"
									"Div2\0"
									"Div4\0"
									"Div16\0\0"},
		{"Hold", 15, 1, CHECKBOX},
		{"Panning", 16, 7, TEXT_BOX},
		{"Wave Duty", 24, 3, TEXT_BOX},
		{"Repeat Mode", 27, 2, COMBO, "Manual\0"
									  "Loop Infinite\0"
									  "One-Shot\0"
									  "Prohibited\0\0"},
		{"Format", 29, 2, COMBO, "PCM8\0"
								 "PCM16\0"
								 "IMA-ADPCM\0"
								 "PSG/Noise\0\0"},
		{"Start/Status", 31, 1, CHECKBOX}}},
	{"SOUND9SAD", "Sound Channel 9 Data Source Register", 0x4000494, 4, false, true, 1, (IoField[]){
		{"Source Address", 0, 27, TEXT_BOX_HEX}}},
	{"SOUND9TMR", "Sound Channel 9 Timer Register", 0x4000498, 2, false, true, 1, (IoField[]){
		{"Timer Value, Sample frequency", 0, 16, TEXT_BOX_HEX}}},
	{"SOUND9PNT", "Sound Channel 9 Loopstart Register", 0x400049A, 2, false, true, 1, (IoField[]){
		{"Loop Start, Sample loop start position", 0, 16, TEXT_BOX_HEX}}},
	{"SOUND9LEN", "Sound Channel 9 Length Register", 0x400049C, 4, false, true, 1, (IoField[]){
		{"Sound length", 0, 22, TEXT_BOX_HEX}}},
	{"SOUNDACNT", "Sound Channel 10 Control Register", 0x40004A0, 4, true, true, 8, (IoField[]){
		{"Volume Mul", 0, 7, TEXT_BOX},
		{"Volume Div", 8, 2, COMBO, "Normal\0"
									"Div2\0"
									"Div4\0"
									"Div16\0\0"},
		{"Hold", 15, 1, CHECKBOX},
		{"Panning", 16, 7, TEXT_BOX},
		{"Wave Duty", 24, 3, TEXT_BOX},
		{"Repeat Mode", 27, 2, COMBO, "Manual\0"
									  "Loop Infinite\0"
									  "One-Shot\0"
									  "Prohibited\0\0"},
		{"Format", 29, 2, COMBO, "PCM8\0"
								 "PCM16\0"
								 "IMA-ADPCM\0"
								 "PSG/Noise\0\0"},
		{"Start/Status", 31, 1, CHECKBOX}}},
	{"SOUNDASAD", "Sound Channel 10 Data Source Register", 0x40004A4, 4, false, true, 1, (IoField[]){
		{"Source Address", 0, 27, TEXT_BOX_HEX}}},
	{"SOUNDATMR", "Sound Channel 10 Timer Register", 0x40004A8, 2, false, true, 1, (IoField[]){
		{"Timer Value, Sample frequency", 0, 16, TEXT_BOX_HEX}}},
	{"SOUNDAPNT", "Sound Channel 10 Loopstart Register", 0x40004AA, 2, false, true, 1, (IoField[]){
		{"Loop Start, Sample loop start position", 0, 16, TEXT_BOX_HEX}}},
	{"SOUNDALEN", "Sound Channel 10 Length Register", 0x40004AC, 4, false, true, 1, (IoField[]){
		{"Sound length", 0, 22, TEXT_BOX_HEX}}},
	{"SOUNDBCNT", "Sound Channel 11 Control Register", 0x40004B0, 4, true, true, 8, (IoField[]){
		{"Volume Mul", 0, 7, TEXT_BOX},
		{"Volume Div", 8, 2, COMBO, "Normal\0"
									"Div2\0"
									"Div4\0"
									"Div16\0\0"},
		{"Hold", 15, 1, CHECKBOX},
		{"Panning", 16, 7, TEXT_BOX},
		{"Wave Duty", 24, 3, TEXT_BOX},
		{"Repeat Mode", 27, 2, COMBO, "Manual\0"
									  "Loop Infinite\0"
									  "One-Shot\0"
									  "Prohibited\0\0"},
		{"Format", 29, 2, COMBO, "PCM8\0"
								 "PCM16\0"
								 "IMA-ADPCM\0"
								 "PSG/Noise\0\0"},
		{"Start/Status", 31, 1, CHECKBOX}}},
	{"SOUNDBSAD", "Sound Channel 11 Data Source Register", 0x40004B4, 4, false, true, 1, (IoField[]){
		{"Source Address", 0, 27, TEXT_BOX_HEX}}},
	{"SOUNDBTMR", "Sound Channel 11 Timer Register", 0x40004B8, 2, false, true, 1, (IoField[]){
		{"Timer Value, Sample frequency", 0, 16, TEXT_BOX_HEX}}},
	{"SOUNDBPNT", "Sound Channel 11 Loopstart Register", 0x40004BA, 2, false, true, 1, (IoField[]){
		{"Loop Start, Sample loop start position", 0, 16, TEXT_BOX_HEX}}},
	{"SOUNDBLEN", "Sound Channel 11 Length Register", 0x40004BC, 4, false, true, 1, (IoField[]){
		{"Sound length", 0, 22, TEXT_BOX_HEX}}},
	{"SOUNDCCNT", "Sound Channel 12 Control Register", 0x40004C0, 4, true, true, 8, (IoField[]){
		{"Volume Mul", 0, 7, TEXT_BOX},
		{"Volume Div", 8, 2, COMBO, "Normal\0"
									"Div2\0"
									"Div4\0"
									"Div16\0\0"},
		{"Hold", 15, 1, CHECKBOX},
		{"Panning", 16, 7, TEXT_BOX},
		{"Wave Duty", 24, 3, TEXT_BOX},
		{"Repeat Mode", 27, 2, COMBO, "Manual\0"
									  "Loop Infinite\0"
									  "One-Shot\0"
									  "Prohibited\0\0"},
		{"Format", 29, 2, COMBO, "PCM8\0"
								 "PCM16\0"
								 "IMA-ADPCM\0"
								 "PSG/Noise\0\0"},
		{"Start/Status", 31, 1, CHECKBOX}}},
	{"SOUNDCSAD", "Sound Channel 12 Data Source Register", 0x40004C4, 4, false, true, 1, (IoField[]){
		{"Source Address", 0, 27, TEXT_BOX_HEX}}},
	{"SOUNDCTMR", "Sound Channel 12 Timer Register", 0x40004C8, 2, false, true, 1, (IoField[]){
		{"Timer Value, Sample frequency", 0, 16, TEXT_BOX_HEX}}},
	{"SOUNDCPNT", "Sound Channel 12 Loopstart Register", 0x40004CA, 2, false, true, 1, (IoField[]){
		{"Loop Start, Sample loop start position", 0, 16, TEXT_BOX_HEX}}},
	{"SOUNDCLEN", "Sound Channel 12 Length Register", 0x40004CC, 4, false, true, 1, (IoField[]){
		{"Sound length", 0, 22, TEXT_BOX_HEX}}},
	{"SOUNDDCNT", "Sound Channel 13 Control Register", 0x40004D0, 4, true, true, 8, (IoField[]){
		{"Volume Mul", 0, 7, TEXT_BOX},
		{"Volume Div", 8, 2, COMBO, "Normal\0"
									"Div2\0"
									"Div4\0"
									"Div16\0\0"},
		{"Hold", 15, 1, CHECKBOX},
		{"Panning", 16, 7, TEXT_BOX},
		{"Wave Duty", 24, 3, TEXT_BOX},
		{"Repeat Mode", 27, 2, TEXT_BOX},
		{"Format", 29, 2, TEXT_BOX},
		{"Start/Status", 31, 1, CHECKBOX}}},
	{"SOUNDDSAD", "Sound Channel 13 Data Source Register", 0x40004D4, 4, false, true, 1, (IoField[]){
		{"Source Address", 0, 27, TEXT_BOX_HEX}}},
	{"SOUNDDTMR", "Sound Channel 13 Timer Register", 0x40004D8, 2, false, true, 1, (IoField[]){
		{"Timer Value, Sample frequency", 0, 16, TEXT_BOX_HEX}}},
	{"SOUNDDPNT", "Sound Channel 13 Loopstart Register", 0x40004DA, 2, false, true, 1, (IoField[]){
		{"Loop Start, Sample loop start position", 0, 16, TEXT_BOX_HEX}}},
	{"SOUNDDLEN", "Sound Channel 13 Length Register", 0x40004DC, 4, false, true, 1, (IoField[]){
		{"Sound length", 0, 22, TEXT_BOX_HEX}}},
	{"SOUNDECNT", "Sound Channel 14 Control Register", 0x40004E0, 4, true, true, 8, (IoField[]){
		{"Volume Mul", 0, 7, TEXT_BOX},
		{"Volume Div", 8, 2, COMBO, "Normal\0"
									"Div2\0"
									"Div4\0"
									"Div16\0\0"},
		{"Hold", 15, 1, CHECKBOX},
		{"Panning", 16, 7, TEXT_BOX},
		{"Wave Duty", 24, 3, TEXT_BOX},
		{"Repeat Mode", 27, 2, TEXT_BOX},
		{"Format", 29, 2, TEXT_BOX},
		{"Start/Status", 31, 1, CHECKBOX}}},
	{"SOUNDESAD", "Sound Channel 14 Data Source Register", 0x40004E4, 4, false, true, 1, (IoField[]){
		{"Source Address", 0, 27, TEXT_BOX_HEX}}},
	{"SOUNDETMR", "Sound Channel 14 Timer Register", 0x40004E8, 2, false, true, 1, (IoField[]){
		{"Timer Value, Sample frequency", 0, 16, TEXT_BOX_HEX}}},
	{"SOUNDEPNT", "Sound Channel 14 Loopstart Register", 0x40004EA, 2, false, true, 1, (IoField[]){
		{"Loop Start, Sample loop start position", 0, 16, TEXT_BOX_HEX}}},
	{"SOUNDELEN", "Sound Channel 14 Length Register", 0x40004EC, 4, false, true, 1, (IoField[]){
		{"Sound length", 0, 22, TEXT_BOX_HEX}}},
	{"SOUNDFCNT", "Sound Channel 15 Control Register", 0x40004F0, 4, true, true, 8, (IoField[]){
		{"Volume Mul", 0, 7, TEXT_BOX},
		{"Volume Div", 8, 2, COMBO, "Normal\0"
									"Div2\0"
									"Div4\0"
									"Div16\0\0"},
		{"Hold", 15, 1, CHECKBOX},
		{"Panning", 16, 7, TEXT_BOX},
		{"Wave Duty", 24, 3, TEXT_BOX},
		{"Repeat Mode", 27, 2, TEXT_BOX},
		{"Format", 29, 2, TEXT_BOX},
		{"Start/Status", 31, 1, CHECKBOX}}},
	{"SOUNDFSAD", "Sound Channel 15 Data Source Register", 0x40004F4, 4, false, true, 1, (IoField[]){
		{"Source Address", 0, 27, TEXT_BOX_HEX}}},
	{"SOUNDFTMR", "Sound Channel 15 Timer Register", 0x40004F8, 2, false, true, 1, (IoField[]){
		{"Timer Value, Sample frequency", 0, 16, TEXT_BOX_HEX}}},
	{"SOUNDFPNT", "Sound Channel 15 Loopstart Register", 0x40004FA, 2, false, true, 1, (IoField[]){
		{"Loop Start, Sample loop start position", 0, 16, TEXT_BOX_HEX}}},
	{"SOUNDFLEN", "Sound Channel 15 Length Register", 0x40004FC, 4, false, true, 1, (IoField[]){
		{"Sound length", 0, 22, TEXT_BOX_HEX}}},
	{"SOUNDCNT", "Sound Control Register", 0x4000500, 2, true, true, 6, (IoField[]){
		{"Master Volume", 0, 7, TEXT_BOX},
		{"Left Output from", 8, 2, COMBO, "Left Mixer\0"
										  "Ch1\0"
										  "Ch3\0"
										  "Ch1+Ch3\0\0"},
		{"Right Output from", 10, 2, COMBO, "Right Mixer\0"
											"Ch1\0"
											"Ch3\0"
											"Ch1+Ch3\0\0"},
		{"Output Ch1 to Mixer", 12, 1, CHECKBOX},
		{"Output Ch3 to Mixer", 13, 1, CHECKBOX},
		{"Master Enable", 15, 1, CHECKBOX}}},
	{"SOUNDBIAS", "Sound Bias Register", 0x4000504, 2, true, true, 1, (IoField[]){
		{"Sound Bias", 0, 10, TEXT_BOX_HEX}}},
	{"SNDCAP0CNT", "Sound Capture 0 Control Register", 0x4000508, 1, true, true, 5, (IoField[]){
		{"Control of Associated Sound Channels", 0, 1, COMBO, "Output Sound Channel 1\0"
															  "Add to Channel 0\0\0"},
		{"Capture Source Selection", 1, 1, COMBO, "Left Mixer\0"
												  "Channel 0/Bugged\0\0"},
		{"Capture Repeat", 2, 1, COMBO, "Loop\0"
										"One-shot\0\0"},
		{"Capture Format", 3, 1, COMBO, "PCM16\0"
										"PCM8\0\0"},
		{"Capture Start/Status", 7, 1, CHECKBOX}}},
	{"SNDCAP1CNT", "Sound Capture 1 Control Register", 0x4000509, 1, true, true, 5, (IoField[]){
		{"Control of Associated Sound Channels", 0, 1, COMBO, "Output Sound Channel 3\0"
															  "Add to Channel 2\0\0"},
		{"Capture Source Selection", 1, 1, COMBO, "Right Mixer\0"
												  "Channel 2/Bugged\0\0"},
		{"Capture Repeat", 2, 1, COMBO, "Loop\0"
										"One-shot\0\0"},
		{"Capture Format", 3, 1, COMBO, "PCM16\0"
										"PCM8\0\0"},
		{"Capture Start/Status", 7, 1, CHECKBOX}}},
	{"SNDCAP0DAD", "Sound Capture 0 Destination Address", 0x4000510, 4, true, true, 1, (IoField[]){
		{"Destination address", 0, 27, TEXT_BOX_HEX}}},
	{"SNDCAP0LEN", "Sound Capture 0 Length", 0x4000514, 2, false, true, 1, (IoField[]){
		{"Buffer length", 0, 16, TEXT_BOX_HEX}}},
	{"SNDCAP1DAD", "Sound Capture 1 Destination Address", 0x4000518, 4, true, true, 1, (IoField[]){
		{"Destination address", 0, 27, TEXT_BOX_HEX}}},
	{"SNDCAP1LEN", "Sound Capture 1 Length", 0x400051C, 2, false, true, 1, (IoField[]){
		{"Buffer length", 0, 16, TEXT_BOX_HEX}}},
}};

void DebugMenu::ioReg7Window() {
	static std::array<void *, 131> registerPointers7 = {
		&ortin.nds.ppu->DISPSTAT7,
		&ortin.nds.ppu->VCOUNT,
		&ortin.nds.nds7->dma->channel[0].DMASAD,
		&ortin.nds.nds7->dma->channel[0].DMADAD,
		&ortin.nds.nds7->dma->channel[0].DMACNT,
		&ortin.nds.nds7->dma->channel[1].DMASAD,
		&ortin.nds.nds7->dma->channel[1].DMADAD,
		&ortin.nds.nds7->dma->channel[1].DMACNT,
		&ortin.nds.nds7->dma->channel[2].DMASAD,
		&ortin.nds.nds7->dma->channel[2].DMADAD,
		&ortin.nds.nds7->dma->channel[2].DMACNT,
		&ortin.nds.nds7->dma->channel[3].DMASAD,
		&ortin.nds.nds7->dma->channel[3].DMADAD,
		&ortin.nds.nds7->dma->channel[3].DMACNT,
		&ortin.nds.nds7->timer->timer[0].TIMCNT_L,
		&ortin.nds.nds7->timer->timer[0].TIMCNT_H,
		&ortin.nds.nds7->timer->timer[1].TIMCNT_L,
		&ortin.nds.nds7->timer->timer[1].TIMCNT_H,
		&ortin.nds.nds7->timer->timer[2].TIMCNT_L,
		&ortin.nds.nds7->timer->timer[2].TIMCNT_H,
		&ortin.nds.nds7->timer->timer[3].TIMCNT_L,
		&ortin.nds.nds7->timer->timer[3].TIMCNT_H,
		&ortin.nds.shared->KEYINPUT,
		&ortin.nds.shared->KEYCNT7,
		&ortin.nds.shared->EXTKEYIN,
		&ortin.nds.nds7->rtc->bus,
		&ortin.nds.ipc->IPCSYNC7,
		&ortin.nds.ipc->IPCFIFOCNT7,
		&ortin.nds.gamecard->AUXSPICNT,
		&ortin.nds.gamecard->ROMCTRL,
		&ortin.nds.gamecard->key2Seed0Low7,
		&ortin.nds.gamecard->key2Seed1Low7,
		&ortin.nds.gamecard->key2Seed0High7,
		&ortin.nds.gamecard->key2Seed1High7,
		&ortin.nds.nds7->spi->SPICNT,
		&ortin.nds.nds7->spi->SPIDATA,
		&ortin.nds.shared->EXMEMSTAT,
		&ortin.nds.nds7->IME,
		&ortin.nds.nds7->IE,
		&ortin.nds.nds7->IF,
		&ortin.nds.ppu->VRAMSTAT,
		&ortin.nds.shared->WRAMCNT,
		&ortin.nds.nds7->HALTCNT,
		&ortin.nds.nds7->apu->channel[0].SOUNDCNT,
		&ortin.nds.nds7->apu->channel[0].SOUNDSAD,
		&ortin.nds.nds7->apu->channel[0].SOUNDTMR,
		&ortin.nds.nds7->apu->channel[0].SOUNDPNT,
		&ortin.nds.nds7->apu->channel[0].SOUNDLEN,
		&ortin.nds.nds7->apu->channel[1].SOUNDCNT,
		&ortin.nds.nds7->apu->channel[1].SOUNDSAD,
		&ortin.nds.nds7->apu->channel[1].SOUNDTMR,
		&ortin.nds.nds7->apu->channel[1].SOUNDPNT,
		&ortin.nds.nds7->apu->channel[1].SOUNDLEN,
		&ortin.nds.nds7->apu->channel[2].SOUNDCNT,
		&ortin.nds.nds7->apu->channel[2].SOUNDSAD,
		&ortin.nds.nds7->apu->channel[2].SOUNDTMR,
		&ortin.nds.nds7->apu->channel[2].SOUNDPNT,
		&ortin.nds.nds7->apu->channel[2].SOUNDLEN,
		&ortin.nds.nds7->apu->channel[3].SOUNDCNT,
		&ortin.nds.nds7->apu->channel[3].SOUNDSAD,
		&ortin.nds.nds7->apu->channel[3].SOUNDTMR,
		&ortin.nds.nds7->apu->channel[3].SOUNDPNT,
		&ortin.nds.nds7->apu->channel[3].SOUNDLEN,
		&ortin.nds.nds7->apu->channel[4].SOUNDCNT,
		&ortin.nds.nds7->apu->channel[4].SOUNDSAD,
		&ortin.nds.nds7->apu->channel[4].SOUNDTMR,
		&ortin.nds.nds7->apu->channel[4].SOUNDPNT,
		&ortin.nds.nds7->apu->channel[4].SOUNDLEN,
		&ortin.nds.nds7->apu->channel[5].SOUNDCNT,
		&ortin.nds.nds7->apu->channel[5].SOUNDSAD,
		&ortin.nds.nds7->apu->channel[5].SOUNDTMR,
		&ortin.nds.nds7->apu->channel[5].SOUNDPNT,
		&ortin.nds.nds7->apu->channel[5].SOUNDLEN,
		&ortin.nds.nds7->apu->channel[6].SOUNDCNT,
		&ortin.nds.nds7->apu->channel[6].SOUNDSAD,
		&ortin.nds.nds7->apu->channel[6].SOUNDTMR,
		&ortin.nds.nds7->apu->channel[6].SOUNDPNT,
		&ortin.nds.nds7->apu->channel[6].SOUNDLEN,
		&ortin.nds.nds7->apu->channel[7].SOUNDCNT,
		&ortin.nds.nds7->apu->channel[7].SOUNDSAD,
		&ortin.nds.nds7->apu->channel[7].SOUNDTMR,
		&ortin.nds.nds7->apu->channel[7].SOUNDPNT,
		&ortin.nds.nds7->apu->channel[7].SOUNDLEN,
		&ortin.nds.nds7->apu->channel[8].SOUNDCNT,
		&ortin.nds.nds7->apu->channel[8].SOUNDSAD,
		&ortin.nds.nds7->apu->channel[8].SOUNDTMR,
		&ortin.nds.nds7->apu->channel[8].SOUNDPNT,
		&ortin.nds.nds7->apu->channel[8].SOUNDLEN,
		&ortin.nds.nds7->apu->channel[9].SOUNDCNT,
		&ortin.nds.nds7->apu->channel[9].SOUNDSAD,
		&ortin.nds.nds7->apu->channel[9].SOUNDTMR,
		&ortin.nds.nds7->apu->channel[9].SOUNDPNT,
		&ortin.nds.nds7->apu->channel[9].SOUNDLEN,
		&ortin.nds.nds7->apu->channel[10].SOUNDCNT,
		&ortin.nds.nds7->apu->channel[10].SOUNDSAD,
		&ortin.nds.nds7->apu->channel[10].SOUNDTMR,
		&ortin.nds.nds7->apu->channel[10].SOUNDPNT,
		&ortin.nds.nds7->apu->channel[10].SOUNDLEN,
		&ortin.nds.nds7->apu->channel[11].SOUNDCNT,
		&ortin.nds.nds7->apu->channel[11].SOUNDSAD,
		&ortin.nds.nds7->apu->channel[11].SOUNDTMR,
		&ortin.nds.nds7->apu->channel[11].SOUNDPNT,
		&ortin.nds.nds7->apu->channel[11].SOUNDLEN,
		&ortin.nds.nds7->apu->channel[12].SOUNDCNT,
		&ortin.nds.nds7->apu->channel[12].SOUNDSAD,
		&ortin.nds.nds7->apu->channel[12].SOUNDTMR,
		&ortin.nds.nds7->apu->channel[12].SOUNDPNT,
		&ortin.nds.nds7->apu->channel[12].SOUNDLEN,
		&ortin.nds.nds7->apu->channel[13].SOUNDCNT,
		&ortin.nds.nds7->apu->channel[13].SOUNDSAD,
		&ortin.nds.nds7->apu->channel[13].SOUNDTMR,
		&ortin.nds.nds7->apu->channel[13].SOUNDPNT,
		&ortin.nds.nds7->apu->channel[13].SOUNDLEN,
		&ortin.nds.nds7->apu->channel[14].SOUNDCNT,
		&ortin.nds.nds7->apu->channel[14].SOUNDSAD,
		&ortin.nds.nds7->apu->channel[14].SOUNDTMR,
		&ortin.nds.nds7->apu->channel[14].SOUNDPNT,
		&ortin.nds.nds7->apu->channel[14].SOUNDLEN,
		&ortin.nds.nds7->apu->channel[15].SOUNDCNT,
		&ortin.nds.nds7->apu->channel[15].SOUNDSAD,
		&ortin.nds.nds7->apu->channel[15].SOUNDTMR,
		&ortin.nds.nds7->apu->channel[15].SOUNDPNT,
		&ortin.nds.nds7->apu->channel[15].SOUNDLEN,
		&ortin.nds.nds7->apu->SOUNDCNT,
		&ortin.nds.nds7->apu->SOUNDBIAS,
		&ortin.nds.nds7->apu->SNDCAP0CNT,
		&ortin.nds.nds7->apu->SNDCAP1CNT,
		&ortin.nds.nds7->apu->SNDCAP0DAD,
		&ortin.nds.nds7->apu->SNDCAP0LEN,
		&ortin.nds.nds7->apu->SNDCAP1DAD,
		&ortin.nds.nds7->apu->SNDCAP1LEN
	};

	static int selected = 0;
	static bool refresh = true;
	static bool tmpRefresh = false;

	ImGui::SetNextWindowSize(ImVec2(500, 440), ImGuiCond_FirstUseEver);
	ImGui::Begin("NDS7 IO Registers", &showIoReg7);

	// Left
	{
		ImGui::BeginChild("left pane", ImVec2(200, 0), true);
		for (int i = 0; i < registers7.size(); i++) {
			auto reg = registers7[i];
			if (ImGui::Selectable(fmt::format("{}: 0x{:0>7X} ({})", reg.name, reg.address, reg.readable == reg.writable ? "R/W" : (reg.readable ? "R" : "W")).c_str(), selected == i)) {
				if (i != selected)
					tmpRefresh = true;
				selected = i;
			}
		}
		ImGui::EndChild();
	}
	ImGui::SameLine();

	// Right
	int size = registers7[selected].size;
	u32 address = registers7[selected].address;
	static u32 value;
	{
		if (refresh || tmpRefresh) {
			if (size == 4) {
				value = *(u32 *)registerPointers7[selected];
			} else if (size == 2) {
				value = *(u16 *)registerPointers7[selected];
			} else {
				value = *(u8 *)registerPointers7[selected];
			}
		}

		ImGui::BeginGroup();
		ImGui::BeginChild("item view", ImVec2(0, -ImGui::GetFrameHeightWithSpacing())); // Leave room for 1 line below us

		ImGui::Text("%s", fmt::format("0x{:0>{}X} - {}", value, size * 2, registers7[selected].description).c_str());
		ImGui::Separator();

		for (int i = 0; i < registers7[selected].numFields; i++) {
			auto field = registers7[selected].fields[i];
			unsigned mask = ((1 << field.length) - 1) << field.startBit;
			u32 fValue = (value & mask) >> field.startBit;

			switch (field.type) {
			case TEXT_BOX:
				fValue = numberInput(((std::string)field.name).c_str(), false, fValue, mask >> field.startBit);
				break;
			case TEXT_BOX_HEX:
				fValue = numberInput(((std::string)field.name).c_str(), true, fValue, mask >> field.startBit);
				break;
			case CHECKBOX:
				ImGui::Checkbox(((std::string)field.name).c_str(), (bool *)&fValue);
				break;
			case COMBO:
				ImGui::Combo(((std::string)field.name).c_str(), (int *)&fValue, field.comboText);
				break;
			case SPECIAL:
				switch (address) {
				case 0x4000004: // DISPSTAT
					fValue = numberInput(((std::string)field.name).c_str(), false, (fValue >> 1) | ((fValue & 1) << 8), mask >> field.startBit);
					fValue = ((fValue & 0xFF) << 1) | (fValue >> 8);
					break;
				}
				break;
			}

			value = (value & ~mask) | (fValue << field.startBit);
		}
		ImGui::EndChild();

		ImGui::Checkbox("Refresh", &refresh);
		ImGui::SameLine();

		tmpRefresh = false;
		if (ImGui::Button("Refresh"))
			tmpRefresh = true;
		ImGui::SameLine();

		if (ImGui::Button("Write")) {
			if (size == 4) {
				ortin.nds.nds7->writeIO(address | 0, (u8)(value >> 0), false);
				ortin.nds.nds7->writeIO(address | 1, (u8)(value >> 8), false);
				ortin.nds.nds7->writeIO(address | 2, (u8)(value >> 16), false);
				ortin.nds.nds7->writeIO(address | 3, (u8)(value >> 24), true);
			} else if (size == 2) {
				ortin.nds.nds7->writeIO(address | 0, (u8)(value >> 0), false);
				ortin.nds.nds7->writeIO(address | 1, (u8)(value >> 8), true);
			} else {
				ortin.nds.nds7->writeIO(address, (u8)value, true);
			}

			tmpRefresh = true;
		}
		ImGui::EndGroup();
	}

	ImGui::End();
}
