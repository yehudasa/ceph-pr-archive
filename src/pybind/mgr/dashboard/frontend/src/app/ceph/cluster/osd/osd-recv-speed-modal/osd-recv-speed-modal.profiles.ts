export class OsdRecvSpeedProfiles {
  public static KNOWN_PROFILES = [
    {
      name: 'none',
      text: '-- Select a profile --',
      values: {
        osd_max_backfills: null,
        osd_recovery_max_active: null,
        osd_recovery_max_single_start: null,
        osd_recovery_sleep: null
      }
    },
    {
      name: 'slow',
      text: 'Slow',
      values: {
        osd_max_backfills: 1,
        osd_recovery_max_active: 1,
        osd_recovery_max_single_start: 1,
        osd_recovery_sleep: 0.5
      }
    },
    {
      name: 'default',
      text: 'Default',
      values: {
        osd_max_backfills: 1,
        osd_recovery_max_active: 3,
        osd_recovery_max_single_start: 1,
        osd_recovery_sleep: 0
      }
    },
    {
      name: 'fast',
      text: 'Fast',
      values: {
        osd_max_backfills: 4,
        osd_recovery_max_active: 4,
        osd_recovery_max_single_start: 4,
        osd_recovery_sleep: 0
      }
    }
  ];
}
