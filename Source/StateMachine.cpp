#include "StateMachine.h"
#include "IState.h" // Include header IState
#include <cstddef> // Untuk nullptr

// Konstruktor: Inisialisasi currentState ke nullptr
StateMachine::StateMachine() : currentState(nullptr)
{
    // Tidak ada isi
}

// Destruktor: Pastikan state terakhir di-OnExit() dan di-delete
StateMachine::~StateMachine()
{
    // Panggil ChangeState dengan nullptr untuk membersihkan state terakhir
    ChangeState(nullptr);
}

void StateMachine::ChangeState(IState* newState)
{
    // 1. Panggil OnExit() pada state lama (jika ada) dan hapus
    if (currentState != nullptr)
    {
        currentState->OnExit();
        delete currentState;
        currentState = nullptr;
    }

    // 2. Atur state baru
    currentState = newState;

    // 3. Panggil OnEnter() pada state baru (jika tidak null)
    if (currentState != nullptr)
    {
        currentState->OnEnter();
    }
}

void StateMachine::Update(float delta_time)
{
    // Panggil Update() pada state yang aktif
    if (currentState != nullptr)
    {
        currentState->Update(delta_time);
    }
}

IState* StateMachine::GetCurrentState() const
{
    return currentState;
}
