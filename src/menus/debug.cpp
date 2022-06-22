
#include "menus/debug.hpp"
#include "imgui_memory_editor.h"

#include <fstream>

DebugMenu::DebugMenu() {
	showLogs = false;
	showArm9Debug = false;
	showArm7Debug = false;
	showMemEditor9 = false;
	showMemEditor7 = false;

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

		ImGui::EndMenu();
	}
}

void DebugMenu::drawWindows() {
	if (showLogs) logsWindow();
	if (showArm9Debug) arm9DebugWindow();
	if (showArm7Debug) arm7DebugWindow();
	if (showMemEditor9) memEditor9Window();
	if (showMemEditor7) memEditor7Window();
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

	/*ImGui::Spacing();
	if (ImGui::Button("Step")) {
		// Add events the hard way so mutex doesn't have to be unlocked
		GBA.cpu.threadQueueMutex.lock();
		GBA.cpu.threadQueue.push(GBACPU::threadEvent{GBACPU::START, 0, nullptr});
		GBA.cpu.threadQueue.push(GBACPU::threadEvent{GBACPU::STOP, 1, nullptr});
		GBA.cpu.threadQueueMutex.unlock();
	}*/

	ImGui::Separator();
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
		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		ImGui::Text("CPSR:");
		ImGui::TableNextColumn();
		ImGui::Text("%08X", cpu.reg.CPSR);
		ImGui::TableNextRow();

		ImGui::EndTable();
	}
	ImGui::Spacing();
	bool n = cpu.reg.flagN; ImGui::Checkbox("N", &n);
	ImGui::SameLine();
	bool z = cpu.reg.flagZ; ImGui::Checkbox("Z", &z);
	ImGui::SameLine();
	bool c = cpu.reg.flagC; ImGui::Checkbox("C", &c);
	ImGui::SameLine();
	bool v = cpu.reg.flagV; ImGui::Checkbox("V", &v);
	ImGui::SameLine();
	bool q = cpu.reg.flagQ; ImGui::Checkbox("Q", &q);
	ImGui::SameLine();
	bool i = cpu.reg.irqDisable; ImGui::Checkbox("I", &i);
	ImGui::SameLine();
	bool f = cpu.reg.fiqDisable; ImGui::Checkbox("F", &f);
	ImGui::SameLine();
	bool t = cpu.reg.thumbMode; ImGui::Checkbox("T", &t);

	/*ImGui::Spacing();
	bool imeTmp = GBA.cpu.IME;
	ImGui::Checkbox("IME", &imeTmp);
	ImGui::SameLine();
	ImGui::Text("IE: %04X", GBA.cpu.IE);
	ImGui::SameLine();
	ImGui::Text("IF: %04X", GBA.cpu.IF);*/

	ImGui::End();
}

void DebugMenu::arm7DebugWindow() {
	auto& cpu = ortin.nds.nds7.cpu;

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

	/*ImGui::Spacing();
	if (ImGui::Button("Step")) {
		// Add events the hard way so mutex doesn't have to be unlocked
		GBA.cpu.threadQueueMutex.lock();
		GBA.cpu.threadQueue.push(GBACPU::threadEvent{GBACPU::START, 0, nullptr});
		GBA.cpu.threadQueue.push(GBACPU::threadEvent{GBACPU::STOP, 1, nullptr});
		GBA.cpu.threadQueueMutex.unlock();
	}*/

	ImGui::Separator();
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
		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		ImGui::Text("CPSR:");
		ImGui::TableNextColumn();
		ImGui::Text("%08X", cpu.reg.CPSR);
		ImGui::TableNextRow();

		ImGui::EndTable();
	}
	ImGui::Spacing();
	bool n = cpu.reg.flagN; ImGui::Checkbox("N", &n);
	ImGui::SameLine();
	bool z = cpu.reg.flagZ; ImGui::Checkbox("Z", &z);
	ImGui::SameLine();
	bool c = cpu.reg.flagC; ImGui::Checkbox("C", &c);
	ImGui::SameLine();
	bool v = cpu.reg.flagV; ImGui::Checkbox("V", &v);
	ImGui::SameLine();
	bool i = cpu.reg.irqDisable; ImGui::Checkbox("I", &i);
	ImGui::SameLine();
	bool f = cpu.reg.fiqDisable; ImGui::Checkbox("F", &f);
	ImGui::SameLine();
	bool t = cpu.reg.thumbMode; ImGui::Checkbox("T", &t);

	/*ImGui::Spacing();
	bool imeTmp = GBA.cpu.IME;
	ImGui::Checkbox("IME", &imeTmp);
	ImGui::SameLine();
	ImGui::Text("IE: %04X", GBA.cpu.IE);
	ImGui::SameLine();
	ImGui::Text("IF: %04X", GBA.cpu.IF);*/

	ImGui::End();
}

void DebugMenu::memEditor9Window() {
	static MemoryEditor memEditor;
	static int selectedRegion = 0;
	std::array<MemoryRegion, 1> regions = {
		{"PSRAM/Main Memory (4MB mirrored 0x2000000 - 0x3000000)", ortin.nds.shared.psram, 0x400000}
	};

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
	std::array<MemoryRegion, 2> regions = {
		(MemoryRegion){"PSRAM/Main Memory (4MB mirrored 0x2000000 - 0x3000000)", ortin.nds.shared.psram, 0x400000},
		(MemoryRegion){"ARM7 WRAM (64KB mirrored 0x3800000 - 0x4000000)", ortin.nds.nds7.wram, 0x10000}
	};

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
