'use strict';

promise_test(async t => {
  const observer1_updates = [];
  const observer1 = new PressureObserver(
      update => { observer1_updates.push(update); },
      {cpuUtilizationThresholds: [0.5]});
  t.add_cleanup(() => observer1.disconnect());
  // Ensure that observer1's quantization scheme gets registered as the frame's
  // scheme before observer2 starts.
  await observer1.observe('cpu');

  const observer2_updates = [];
  await new Promise((resolve, reject) => {
    const observer2 = new PressureObserver(
        update => {
          observer2_updates.push(update);
          resolve();
        },
        {cpuUtilizationThresholds: [0.25]});
    t.add_cleanup(() => observer2.disconnect());
    observer2.observe('cpu').catch(reject);
  });

  // observer2 uses a different quantization scheme than observer1. After
  // observer2.observe() completes, observer1 should no longer be active.
  //
  // The check below assumes that observer2.observe() completes before the
  // browser dispatches any update for observer1.  This assumption is highly
  // likely to be true, because there should be a 1-second delay between
  // observer1.observe() and the first update that observer1 would receive.
  assert_equals(
      observer1_updates.length, 0,
      'observer2.observe() should have stopped observer1; the two observers ' +
      'have different quantization schemes');

  assert_equals(observer2_updates.length, 1);
  assert_in_array(observer2_updates[0].cpuUtilization, [0.125, 0.625],
                  'cpuUtilization quantization');

  // Go through one more update cycle so any (incorrect) update for observer1
  // makes it through the IPC queues.
  observer1_updates.length = 0;
  observer2_updates.length = 0;

  const observer3_updates = [];
  await new Promise((resolve, reject) => {
    const observer3 = new PressureObserver(
        update => {
          observer3_updates.push(update);
          resolve();
        },
        {cpuUtilizationThresholds: [0.75]});
    t.add_cleanup(() => observer3.disconnect());
    observer3.observe('cpu').catch(reject);
  });

  assert_equals(
      observer1_updates.length, 0,
      'observer2.observe() should have stopped observer1; the two observers ' +
      'have different quantization schemes');

  // observer3 uses a different quantization scheme than observer2. So,
  // observer3.observe() should stop observer2.
  assert_equals(
      observer2_updates.length, 0,
      'observer3.observe() should have stopped observer2; the two observers ' +
      'have different quantization schemes');

  assert_equals(observer3_updates.length, 1);
  assert_in_array(observer3_updates[0].cpuUtilization, [0.375, 0.875],
                  'cpuUtilization quantization');
}, 'PressureObserver with a new quantization schema stops all ' +
   'other active observers in the same frame');
