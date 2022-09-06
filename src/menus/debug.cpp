
#include "menus/debug.hpp"
#include "imgui_memory_editor.h"

#include <fstream>

DebugMenu::DebugMenu() {
	showLogs = false;
	showArm9Debug = false;
	showArm7Debug = false;
	showMemEditor9 = false;
	showMemEditor7 = false;
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
		ImGui::MenuItem("ARM9 Debug", nullptr, &showArm9Debug);
		ImGui::MenuItem("ARM7 Debug", nullptr, &showArm7Debug);
		ImGui::MenuItem("ARM9 Memory", nullptr, &showMemEditor9);
		ImGui::MenuItem("ARM7 Memory", nullptr, &showMemEditor7);
		ImGui::MenuItem("ARM9 IO", nullptr, &showIoReg9);
		ImGui::MenuItem("ARM7 IO", nullptr, &showIoReg7);

		ImGui::EndMenu();
	}
}

void DebugMenu::drawWindows() {
	if (showLogs) logsWindow();
	if (showArm9Debug) arm9DebugWindow();
	if (showArm7Debug) arm7DebugWindow();
	if (showMemEditor9) memEditor9Window();
	if (showMemEditor7) memEditor7Window();
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

struct MemoryRegion {
	std::string name;
	u8 *pointer;
	int size;
};

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
		systemLogFileStream << ortin.nds.log.str();
		systemLogFileStream.close();
	}

	ImGui::Checkbox("Log DMA9", &ortin.nds.nds9.dma.logDma);
	ImGui::SameLine();
	ImGui::Checkbox("Log DMA7", &ortin.nds.nds7.dma.logDma);

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
		ImGui::TextUnformatted(ortin.nds.log.str().c_str());
		if (shouldAutoscroll)
			ImGui::SetScrollHereY(1.0f);
		ImGui::EndChild();
		ImGui::TreePop();
	}

	ImGui::End();
}

void DebugMenu::arm9DebugWindow() {
	auto& cpu = ortin.nds.nds9.cpu;

	ImGui::SetNextWindowSize(ImVec2(760, 480));
	ImGui::Begin("ARM9 Debug", &showArm9Debug);

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

		std::string tmp = arm9disasm.disassemble(cpu.reg.R[15] - (cpu.reg.thumbMode ? 4 : 8), cpu.pipelineOpcode3, cpu.reg.thumbMode);
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
			ImGui::Text("%08X", cpu.reg.R[0]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r1:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[1]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r2:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[2]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r3:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[3]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r4:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[4]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r5:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[5]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r6:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[6]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r7:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[7]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r8:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R8_user);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R8_fiq);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r9:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[9]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R9_user);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R9_fiq);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r10:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[10]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R10_user);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R10_fiq);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r11:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[11]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R11_user);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R11_fiq);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r12:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[12]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R12_user);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R12_fiq);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r13:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[13]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R13_user);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R13_fiq);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R13_irq);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R13_svc);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R13_abt);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R13_und);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r14:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[14]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R14_user);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R14_fiq);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R14_irq);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R14_svc);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R14_abt);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R14_und);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r15:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[15]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("SPSR:");
			ImGui::TableSetColumnIndex(3);
			ImGui::Text("%08X", cpu.reg.SPSR_fiq);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.SPSR_irq);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.SPSR_svc);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.SPSR_abt);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.SPSR_und);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("CPSR:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.CPSR);
			ImGui::TableNextRow();

			ImGui::EndTable();
		}
		ImGui::Spacing();
		bool n = cpu.reg.flagN;
		ImGui::Checkbox("N", &n);
		ImGui::SameLine();
		bool z = cpu.reg.flagZ;
		ImGui::Checkbox("Z", &z);
		ImGui::SameLine();
		bool c = cpu.reg.flagC;
		ImGui::Checkbox("C", &c);
		ImGui::SameLine();
		bool v = cpu.reg.flagV;
		ImGui::Checkbox("V", &v);
		ImGui::SameLine();
		bool q = cpu.reg.flagQ;
		ImGui::Checkbox("Q", &q);
		ImGui::SameLine();
		bool i = cpu.reg.irqDisable;
		ImGui::Checkbox("I", &i);
		ImGui::SameLine();
		bool f = cpu.reg.fiqDisable;
		ImGui::Checkbox("F", &f);
		ImGui::SameLine();
		bool t = cpu.reg.thumbMode;
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
				cpu.addBreakpoint(breakpointAddress);
				breakpoints.push_back(breakpointAddress);
			}
		}

		if (ImGui::Button("Delete Selected")) {
			if (selectedBreakpoint != -1) {
				cpu.removeBreakpoint(breakpoints[selectedBreakpoint]);
				breakpoints.erase(breakpoints.begin() + selectedBreakpoint);
				selectedBreakpoint = -1;
			}
		}

		for (int i = 0; i < breakpoints.size(); i++) {
			if (ImGui::Selectable(fmt::format("0x{:0>7X}", breakpoints[i]).c_str(), selectedBreakpoint == i))
				selectedBreakpoint = i;
		}

		ImGui::EndChild();
	}

	ImGui::End();
}

void DebugMenu::arm7DebugWindow() {
	auto& cpu = ortin.nds.nds7.cpu;

	ImGui::SetNextWindowSize(ImVec2(760, 480));
	ImGui::Begin("ARM7 Debug", &showArm7Debug);

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

		std::string tmp = arm9disasm.disassemble(cpu.reg.R[15] - (cpu.reg.thumbMode ? 4 : 8), cpu.pipelineOpcode3, cpu.reg.thumbMode);
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
			ImGui::Text("%08X", cpu.reg.R[0]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r1:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[1]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r2:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[2]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r3:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[3]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r4:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[4]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r5:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[5]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r6:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[6]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r7:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[7]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r8:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R8_user);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R8_fiq);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r9:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[9]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R9_user);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R9_fiq);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r10:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[10]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R10_user);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R10_fiq);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r11:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[11]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R11_user);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R11_fiq);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r12:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[12]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R12_user);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R12_fiq);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r13:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[13]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R13_user);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R13_fiq);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R13_irq);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R13_svc);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R13_abt);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R13_und);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r14:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[14]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R14_user);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R14_fiq);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R14_irq);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R14_svc);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R14_abt);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R14_und);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r15:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[15]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("SPSR:");
			ImGui::TableSetColumnIndex(3);
			ImGui::Text("%08X", cpu.reg.SPSR_fiq);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.SPSR_irq);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.SPSR_svc);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.SPSR_abt);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.SPSR_und);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("CPSR:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.CPSR);
			ImGui::TableNextRow();

			ImGui::EndTable();
		}
		ImGui::Spacing();
		bool n = cpu.reg.flagN;
		ImGui::Checkbox("N", &n);
		ImGui::SameLine();
		bool z = cpu.reg.flagZ;
		ImGui::Checkbox("Z", &z);
		ImGui::SameLine();
		bool c = cpu.reg.flagC;
		ImGui::Checkbox("C", &c);
		ImGui::SameLine();
		bool v = cpu.reg.flagV;
		ImGui::Checkbox("V", &v);
		ImGui::SameLine();
		bool i = cpu.reg.irqDisable;
		ImGui::Checkbox("I", &i);
		ImGui::SameLine();
		bool f = cpu.reg.fiqDisable;
		ImGui::Checkbox("F", &f);
		ImGui::SameLine();
		bool t = cpu.reg.thumbMode;
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
				cpu.addBreakpoint(breakpointAddress);
				breakpoints.push_back(breakpointAddress);
			}
		}

		if (ImGui::Button("Delete Selected")) {
			if (selectedBreakpoint != -1) {
				cpu.removeBreakpoint(breakpoints[selectedBreakpoint]);
				breakpoints.erase(breakpoints.begin() + selectedBreakpoint);
				selectedBreakpoint = -1;
			}
		}

		for (int i = 0; i < breakpoints.size(); i++) {
			if (ImGui::Selectable(fmt::format("0x{:0>7X}", breakpoints[i]).c_str(), selectedBreakpoint == i))
				selectedBreakpoint = i;
		}

		ImGui::EndChild();
	}

	ImGui::End();
}

void DebugMenu::memEditor9Window() {
	static MemoryEditor memEditor;
	static int selectedRegion = 0;
	std::array<MemoryRegion, 1> regions = {{
		{"PSRAM/Main Memory (4MB mirrored 0x2000000 - 0x3000000)", ortin.nds.shared.psram, 0x400000}
	}};

	ImGui::SetNextWindowSize(ImVec2(570, 400), ImGuiCond_FirstUseEver);
	ImGui::Begin("Memory Editor ARM9", &showMemEditor9);

	if (ImGui::BeginCombo("Location", regions[selectedRegion].name.c_str())) {
		for (int i = 0; i < regions.size(); i++) {
			if (ImGui::MenuItem(regions[selectedRegion].name.c_str()))
				selectedRegion = i;
		}

		ImGui::EndCombo();
	}

	memEditor.DrawContents(regions[selectedRegion].pointer, regions[selectedRegion].size);

	ImGui::End();
}

void DebugMenu::memEditor7Window() {
	static MemoryEditor memEditor;
	static int selectedRegion = 0;
	std::array<MemoryRegion, 3> regions = {{
		{"PSRAM/Main Memory (4MB mirrored 0x2000000 - 0x3000000)", ortin.nds.shared.psram, 0x400000},
		{"Shared WRAM (32KB mirrored 0x3000000 - 0x3800000)", ortin.nds.shared.wram, 0x8000},
		{"ARM7 WRAM (64KB mirrored 0x3800000 - 0x4000000)", ortin.nds.nds7.wram, 0x10000}
	}};

	ImGui::SetNextWindowSize(ImVec2(570, 400), ImGuiCond_FirstUseEver);
	ImGui::Begin("Memory Editor ARM7", &showMemEditor7);

	if (ImGui::BeginCombo("Location", regions[selectedRegion].name.c_str())) {
		for (int i = 0; i < regions.size(); i++) {
			if (ImGui::MenuItem(regions[i].name.c_str()))
				selectedRegion = i;
		}

		ImGui::EndCombo();
	}

	memEditor.DrawContents(regions[selectedRegion].pointer, regions[selectedRegion].size);

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

static const std::array<IoRegister, 66> registers9 = {{
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
	{"IME",	"Interrupt Master Enable", 0x4000208, 4, true, true, 1, (IoField[]){
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
	{"(B) MASTER_BRIGHT", "Master Brightness Up/Down", 0x400106C, 2, true, true, 2, (IoField[]){
		{"Factor used for 6bit R,G,B Intensities", 0, 5, TEXT_BOX},
		{"Mode", 14, 2, COMBO, "Disable\0"
							   "Up\0"
							   "Down\0"
							   "Reserved\0\0"}}},
}};

void DebugMenu::ioReg9Window() { // Shamefully stolen from the ImGui demo
	static std::array<void *, 66> registerPointers9 = {
		&ortin.nds.ppu.engineA.DISPCNT,
		&ortin.nds.ppu.DISPSTAT9,
		&ortin.nds.ppu.VCOUNT,
		&ortin.nds.ppu.engineA.bg[0].BGCNT,
		&ortin.nds.ppu.engineA.bg[1].BGCNT,
		&ortin.nds.ppu.engineA.bg[2].BGCNT,
		&ortin.nds.ppu.engineA.bg[3].BGCNT,
		&ortin.nds.ppu.engineA.bg[0].BGHOFS,
		&ortin.nds.ppu.engineA.bg[0].BGVOFS,
		&ortin.nds.ppu.engineA.bg[1].BGHOFS,
		&ortin.nds.ppu.engineA.bg[1].BGVOFS,
		&ortin.nds.ppu.engineA.bg[2].BGHOFS,
		&ortin.nds.ppu.engineA.bg[2].BGVOFS,
		&ortin.nds.ppu.engineA.bg[3].BGHOFS,
		&ortin.nds.ppu.engineA.bg[3].BGVOFS,
		&ortin.nds.ppu.engineA.MASTER_BRIGHT,
		&ortin.nds.nds9.dma.channel[0].DMASAD,
		&ortin.nds.nds9.dma.channel[0].DMADAD,
		&ortin.nds.nds9.dma.channel[0].DMACNT,
		&ortin.nds.nds9.dma.channel[1].DMASAD,
		&ortin.nds.nds9.dma.channel[1].DMADAD,
		&ortin.nds.nds9.dma.channel[1].DMACNT,
		&ortin.nds.nds9.dma.channel[2].DMASAD,
		&ortin.nds.nds9.dma.channel[2].DMADAD,
		&ortin.nds.nds9.dma.channel[2].DMACNT,
		&ortin.nds.nds9.dma.channel[3].DMASAD,
		&ortin.nds.nds9.dma.channel[3].DMADAD,
		&ortin.nds.nds9.dma.channel[3].DMACNT,
		&ortin.nds.nds9.dma.DMA0FILL,
		&ortin.nds.nds9.dma.DMA1FILL,
		&ortin.nds.nds9.dma.DMA2FILL,
		&ortin.nds.nds9.dma.DMA3FILL,
		&ortin.nds.shared.KEYINPUT,
		&ortin.nds.shared.KEYCNT9,
		&ortin.nds.ipc.IPCSYNC9,
		&ortin.nds.ipc.IPCFIFOCNT9,
		&ortin.nds.nds9.IME,
		&ortin.nds.nds9.IE,
		&ortin.nds.nds9.IF,
		&ortin.nds.ppu.VRAMCNT_A,
		&ortin.nds.ppu.VRAMCNT_B,
		&ortin.nds.ppu.VRAMCNT_C,
		&ortin.nds.ppu.VRAMCNT_D,
		&ortin.nds.ppu.VRAMCNT_E,
		&ortin.nds.ppu.VRAMCNT_F,
		&ortin.nds.ppu.VRAMCNT_G,
		&ortin.nds.shared.WRAMCNT,
		&ortin.nds.ppu.VRAMCNT_H,
		&ortin.nds.ppu.VRAMCNT_I,
		&ortin.nds.nds9.dsmath.DIVCNT,
		&ortin.nds.nds9.dsmath.SQRTCNT,
		&ortin.nds.nds9.dsmath.SQRT_RESULT,
		&ortin.nds.ppu.engineB.DISPCNT,
		&ortin.nds.ppu.engineB.bg[0].BGCNT,
		&ortin.nds.ppu.engineB.bg[1].BGCNT,
		&ortin.nds.ppu.engineB.bg[2].BGCNT,
		&ortin.nds.ppu.engineB.bg[3].BGCNT,
		&ortin.nds.ppu.engineB.bg[0].BGHOFS,
		&ortin.nds.ppu.engineB.bg[0].BGVOFS,
		&ortin.nds.ppu.engineB.bg[1].BGHOFS,
		&ortin.nds.ppu.engineB.bg[1].BGVOFS,
		&ortin.nds.ppu.engineB.bg[2].BGHOFS,
		&ortin.nds.ppu.engineB.bg[2].BGVOFS,
		&ortin.nds.ppu.engineB.bg[3].BGHOFS,
		&ortin.nds.ppu.engineB.bg[3].BGVOFS,
		&ortin.nds.ppu.engineB.MASTER_BRIGHT,
	};

	static int selected = 0;
	static bool refresh = true;
	static bool tmpRefresh = false;

	ImGui::SetNextWindowSize(ImVec2(500, 440), ImGuiCond_FirstUseEver);
	ImGui::Begin("ARM9 IO Registers", &showIoReg9);

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
				ortin.nds.nds9.writeIO(address | 0, (u8)(value >> 0), false);
				ortin.nds.nds9.writeIO(address | 1, (u8)(value >> 8), false);
				ortin.nds.nds9.writeIO(address | 2, (u8)(value >> 16), false);
				ortin.nds.nds9.writeIO(address | 3, (u8)(value >> 24), true);
			} else if (size == 2) {
				ortin.nds.nds9.writeIO(address | 0, (u8)(value >> 0), false);
				ortin.nds.nds9.writeIO(address | 1, (u8)(value >> 8), true);
			} else {
				ortin.nds.nds9.writeIO(address, (u8)value, true);
			}

			tmpRefresh = true;
		}
		ImGui::EndGroup();
	}

	ImGui::End();
}

static const std::array<IoRegister, 27> registers7 = {{
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
}};

void DebugMenu::ioReg7Window() {
	static std::array<void *, 25> registerPointers7 = {
		&ortin.nds.ppu.DISPSTAT7,
		&ortin.nds.ppu.VCOUNT,
		&ortin.nds.nds7.dma.channel[0].DMASAD,
		&ortin.nds.nds7.dma.channel[0].DMADAD,
		&ortin.nds.nds7.dma.channel[0].DMACNT,
		&ortin.nds.nds7.dma.channel[1].DMASAD,
		&ortin.nds.nds7.dma.channel[1].DMADAD,
		&ortin.nds.nds7.dma.channel[1].DMACNT,
		&ortin.nds.nds7.dma.channel[2].DMASAD,
		&ortin.nds.nds7.dma.channel[2].DMADAD,
		&ortin.nds.nds7.dma.channel[2].DMACNT,
		&ortin.nds.nds7.dma.channel[3].DMASAD,
		&ortin.nds.nds7.dma.channel[3].DMADAD,
		&ortin.nds.nds7.dma.channel[3].DMACNT,
		&ortin.nds.shared.KEYINPUT,
		&ortin.nds.shared.KEYCNT7,
		&ortin.nds.shared.EXTKEYIN,
		&ortin.nds.ipc.IPCSYNC7,
		&ortin.nds.ipc.IPCFIFOCNT7,
		&ortin.nds.nds7.IME,
		&ortin.nds.nds7.IE,
		&ortin.nds.nds7.IF,
		&ortin.nds.ppu.VRAMSTAT,
		&ortin.nds.shared.WRAMCNT,
		&ortin.nds.nds7.HALTCNT
	};

	static int selected = 0;
	static bool refresh = true;
	static bool tmpRefresh = false;

	ImGui::SetNextWindowSize(ImVec2(500, 440), ImGuiCond_FirstUseEver);
	ImGui::Begin("ARM7 IO Registers", &showIoReg7);

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
				ortin.nds.nds7.writeIO(address | 0, (u8)(value >> 0), false);
				ortin.nds.nds7.writeIO(address | 1, (u8)(value >> 8), false);
				ortin.nds.nds7.writeIO(address | 2, (u8)(value >> 16), false);
				ortin.nds.nds7.writeIO(address | 3, (u8)(value >> 24), true);
			} else if (size == 2) {
				ortin.nds.nds7.writeIO(address | 0, (u8)(value >> 0), false);
				ortin.nds.nds7.writeIO(address | 1, (u8)(value >> 8), true);
			} else {
				ortin.nds.nds7.writeIO(address, (u8)value, true);
			}

			tmpRefresh = true;
		}
		ImGui::EndGroup();
	}

	ImGui::End();
}
