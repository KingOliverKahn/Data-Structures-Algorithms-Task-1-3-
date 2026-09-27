# Task 1: Modern Parking System Analysis and Design

## Introduction

The client wants to automate parking operations in Kenya. Drivers should be able to see available parking spaces before entering, vehicles should be recorded when they arrive, and the system should calculate parking duration and the amount payable when a vehicle exits. After successful payment, the exit barrier should open and the parking space should become available again.

The proposed design separates the system into modules so that each part has a clear responsibility while still working together as one parking system.

## Proposed Modules

### Parking Slot Management

This module keeps track of all parking spaces and their current status. It displays available and occupied spaces, assigns an available space to an arriving vehicle and releases the space when the vehicle leaves.

### Vehicle Management

This module records vehicle details such as registration number and vehicle type. The registration number is used to identify the vehicle during its parking session and at exit.

### Parking Session Management

A parking session begins when a vehicle enters and ends when the vehicle completes payment and leaves. The session stores the vehicle, assigned slot, entry time, exit time, duration and current status.

### Fee Calculation

This module calculates the amount payable from the total parking duration. The implementation uses the fee bands supplied by the client.

### Payment

This module records the amount paid and the time of payment. Payment must be successful before the exit barrier is authorised.

### Barrier Control

This module represents the exit barrier. The barrier remains closed until payment is completed. Once payment succeeds, the system authorises the barrier to open.

### Database Management

The database stores vehicles, parking slots, parking sessions and payments. It also maintains relationships between the records and prevents invalid data where possible.

### Web Interface

The web interface provides the user-facing part of the system. It displays slot availability, provides vehicle arrival and exit functions, shows calculated charges and displays parking records.

## Actors and Main Use Cases

The main actors are the driver, parking management and the system itself.

The main use cases are viewing parking availability, registering a vehicle, assigning a slot, creating a parking session, requesting vehicle exit, calculating duration, calculating the parking fee, processing payment, authorising the exit barrier, releasing a parking slot, viewing parking records and monitoring parking status.

## Algorithms

### Display Available Parking Slots

1. Start.
2. Retrieve all parking slots from the system.
3. Check the status of each slot.
4. Display every slot and identify whether it is available or occupied.
5. Display the total number of available slots.
6. Stop.

### Register Vehicle Arrival

1. Start.
2. Receive the vehicle registration number and vehicle type.
3. Validate the registration number.
4. Check whether the vehicle already has an active parking session.
5. If it does, reject the new arrival.
6. Find an available parking slot.
7. If no slot is available, inform the driver that parking is full.
8. Otherwise create or retrieve the vehicle record.
9. Create a new parking session with the current entry time.
10. Mark the selected slot as occupied.
11. Display the assigned slot.
12. Stop.

### Find an Available Slot

1. Start.
2. Search the parking slots.
3. Select a slot whose status is available.
4. Return the selected slot.
5. If no available slot exists, return no result.
6. Stop.

### Calculate Parking Duration

1. Start.
2. Retrieve the entry time for the active parking session.
3. Obtain the exit time.
4. Calculate the difference between the two times in minutes.
5. Return the duration.
6. Stop.

### Calculate Parking Fee

1. Start.
2. Receive the total parking duration in minutes.
3. If the duration is 30 minutes or less, set the fee to KSh 0.
4. If the duration is more than 30 minutes and up to 120 minutes, set the fee to KSh 50.
5. If the duration is more than 120 minutes and up to 240 minutes, set the fee to KSh 100.
6. If the duration is more than 240 minutes and up to 360 minutes, set the fee to KSh 300.
7. If the duration is more than 360 minutes, set the fee to KSh 500.
8. Return the fee.
9. Stop.

### Process Vehicle Exit

1. Start.
2. Receive the vehicle registration number.
3. Find the vehicle's active parking session.
4. If no active session exists, report that the vehicle is not currently parked.
5. Calculate the parking duration.
6. Calculate the amount payable.
7. Display the duration and amount.
8. Wait for successful payment.
9. Record the payment.
10. Complete the parking session and record the exit time.
11. Release the assigned parking slot.
12. Authorise the barrier to open.
13. Stop.

## Parking Fee Structure

| Parking duration | Fee |
|---|---:|
| Up to 30 minutes | KSh 0 |
| More than 30 minutes up to 2 hours | KSh 50 |
| More than 2 hours up to 4 hours | KSh 100 |
| More than 4 hours up to 6 hours | KSh 300 |
| More than 6 hours | KSh 500 |

The boundary conditions are handled explicitly. For example, exactly 30 minutes is free, while 31 minutes falls into the KSh 50 band.

## Data Structures

A vector or similar sequential structure is suitable for maintaining the collection of parking slots because the system needs to inspect the slots and display their status.

An unordered map can be used to provide fast lookup of active vehicles by registration number. This is useful when a vehicle arrives or requests to exit.

A queue can be introduced if the client later requires a waiting list for vehicles when the parking area is full.

A stack is appropriate where the system genuinely needs last-in-first-out behaviour. The factorial problem in Task 3 provides the main demonstration of this structure. It should not be forced into the parking system simply for the sake of using every possible data structure.

## Dynamic Database Design

The system uses four main entities.

### VEHICLE

Stores vehicle information.

`vehicle_id` is the primary key. `registration_number` is unique and identifies the vehicle. `vehicle_type` stores the type of vehicle and `created_at` records when the vehicle was first registered.

### PARKING_SLOT

Stores the available parking spaces.

`slot_id` is the primary key. `slot_number` uniquely identifies the physical slot. `slot_type` can be used for different categories of spaces and `status` records whether a slot is available or occupied.

### PARKING_SESSION

Stores each parking visit.

The table links a vehicle to its assigned parking slot and records entry time, exit time, duration, amount due and session status.

### PAYMENT

Stores payments made for completed parking sessions. It records the session, amount, payment time and payment status.

The main relationships are one vehicle to many parking sessions, one parking slot to many parking sessions over time, and one parking session to zero or one payment.

## Database Integrity

The database should prevent duplicate vehicle registration numbers, invalid slot statuses and invalid session states. Foreign keys should ensure that sessions refer to existing vehicles and parking slots and that payments refer to existing sessions. An active parking slot should not be assigned to more than one active session at the same time.

Indexes on vehicle registration numbers and active parking sessions improve lookup when processing arrivals and exits.

## Proposed System Structure

The system follows a simple web-based structure:

Web browser → C++ server → parking system logic → SQLite database

The browser handles presentation and user interaction. The C++ backend performs validation, parking operations, fee calculation and payment processing. SQLite provides persistent storage.

## Conclusion

The proposed design separates the parking system into manageable modules while keeping them connected as one application. The design covers the client's requirements from viewing available spaces and recording arrivals to calculating fees, processing payment, opening the barrier and releasing the parking space. The design also provides a database structure that can store the system's records dynamically and supports the implementation developed in Task 2.
