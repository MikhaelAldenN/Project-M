#pragma once

// Forward declaration untuk menghindari include sirkular jika IState butuh StateMachine
class IState;

/**
 * @file StateMachine.h
 * @brief [Fondasi FSM] Poin 2 To-Do List GDD.
 * Class ini memegang state saat ini (currentState) dan mengelola
 * logika transisi antar state (ChangeState).
 * Setiap entitas yang butuh FSM (Player, Enemy) akan memiliki
 * instance dari StateMachine ini.
 */
class StateMachine
{
public:
    /**
     * @brief Konstruktor. Menginisialisasi state awal ke null.
     */
    StateMachine();

    /**
     * @brief Destruktor. Memastikan state saat ini dihapus dengan aman.
     */
    ~StateMachine();

    /**
     * @brief Mengganti state saat ini dengan state yang baru.
     * Ini adalah inti dari FSM.
     * @param newState Pointer ke instance state baru yang akan diaktifkan.
     * StateMachine akan mengambil alih memori (delete) state ini.
     */
    void ChangeState(IState* newState);

    /**
     * @brief Memanggil Update() pada state yang sedang aktif.
     * Ini akan dipanggil oleh Player::Update() atau Enemy::Update().
     * @param delta_time Waktu (dalam detik) sejak frame terakhir.
     */
    void Update(float delta_time);

    /**
     * @brief (Opsional) Mengambil state yang sedang berjalan saat ini.
     * Berguna untuk debugging atau UI.
     * @return Pointer ke state yang sedang aktif.
     */
    IState* GetCurrentState() const;

private:
    // Pointer ke state yang sedang aktif saat ini.
    IState* currentState;

    // Mencegah copy constructor dan assignment operator
    // FSM tidak boleh di-copy secara sembarangan
    StateMachine(const StateMachine&) = delete;
    StateMachine& operator=(const StateMachine&) = delete;
};
